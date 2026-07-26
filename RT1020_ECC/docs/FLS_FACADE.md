# MCAL Fls facade — 비동기 job 모델 (F-5)

> 지금까지(F-2 ~ F-4)는 FlexSPI 명령으로 flash 를 읽고 지우고 쓰는 **하드웨어 드라이버**를
> 만들었다. 이 문서는 그 위에 얹는 **AUTOSAR 표준 API(MCAL Fls)** 를 다룬다.
> 핵심은 하드웨어가 아니라 **설계** 다 — 수십 ms 걸리는 연산을 블로킹 없이 다루는
> 비동기 job 모델과, 단일 flash XIP 라는 물리적 제약을 어떻게 정직하게 타협했는가.

---

## Part 1. 큰 그림 — MCAL 스택에서 Fls 의 위치

```
┌─────────────── 상위 (Fee / NvM / 애플리케이션) ───────────────┐
│  "이 블록을 저장해줘"  →  주소·크기 신경 안 씀                  │
└───────────────────────────────┬───────────────────────────────┘
                                 │  MemIf 공통 인터페이스
┌───────────────────────────────▼───────────────────────────────┐
│  Fls (이 문서)  — AUTOSAR 표준 API + 비동기 job + 영역 보호     │
│    Fls_Erase / Fls_Write / Fls_Read / Fls_MainFunction         │
└───────────────────────────────┬───────────────────────────────┘
                                 │  블로킹 프리미티브 호출
┌───────────────────────────────▼───────────────────────────────┐
│  flexspi_ip (F-2 ~ F-4)  — FlexSPI IP 명령으로 flash 직접 제어  │
│    Fls_EraseSector / Fls_ProgramPage / FlexSPI_ReadData        │
└────────────────────────────────────────────────────────────────┘
```

Fls 는 **위(상위 스택)와 아래(하드웨어 드라이버) 사이의 통역** 이다. 위에는 "주소·크기·타이밍을
숨긴 깔끔한 job API" 를 제공하고, 아래로는 F-4 에서 만든 프리미티브를 호출한다. 이 문서에서
만드는 것은 그 통역 계층이다.

---

## Part 2. 왜 비동기 job 모델인가 — 40ms 를 블로킹으로 기다릴 수 없다

sector erase 는 **수십 ms** 가 걸린다(F-4b 실측: 폴링 8만 회 ≈ 40ms). 만약 `Fls_Erase()` 가
그 40ms 를 모두 기다린 뒤에야 반환하는 블로킹 함수라면, 그동안 CPU 는 다른 일을 전혀 하지
못한다. 통신 스택도, 제어 루프도, 워치독 갱신도 모두 멈춘다. 실시간 시스템에서 이것은 허용되지
않는다.

그래서 AUTOSAR 는 메모리 드라이버를 **"요청과 진행을 분리"** 하는 job 모델로 설계했다.

```
Fls_Erase(addr, len)   요청을 접수하고 곧바로 반환한다 (E_OK)
                       내부 상태: MEMIF_BUSY, 결과: MEMIF_JOB_PENDING
Fls_MainFunction()     스케줄러가 주기적으로 부르며, 작업을 한 조각씩 진행한다
Fls_GetJobResult()     완료 여부를 조회한다: PENDING / OK / FAILED
```

호출자는 `Fls_Erase()` 를 부른 뒤 자기 일을 계속하다가, 이따금 `Fls_GetJobResult()` 로 완료를
확인한다. **"명령을 쏘고 완료를 폴링한다"** 는 이 흐름은 F-3 의 WIP 폴링과 정확히 같은 철학인데,
이번에는 그것을 **드라이버 API 수준으로 끌어올린** 것이다.

---

## Part 3. XIP 의 근본 제약 — "완전한 비동기" 는 불가능하다

이 챕터의 마지막 지적 고비가 여기에 있다. 순진하게 생각하면 비동기는 이래야 한다.

```
Fls_Erase()         WREN 과 erase 명령만 쏘고 즉시 반환한다 (flash 는 이제 busy)
Fls_MainFunction()  WIP 를 한 번 읽고 반환한다. 아직 busy 면 그냥 돌아간다
... 그 사이 main 루프는 자기 일을 한다 ...
```

**그런데 이 방식은 우리 XIP 환경에서 동작하지 않는다.** flash 가 erase 중(WIP=1)일 때 main
루프가 자기 코드를 실행하려면, 그 코드는 flash 에 있으니 fetch 해야 한다. 하지만 flash 는 지금
지우느라 읽기를 내주지 않는다. F-4c 에서 겪은 그 `IBUSERR` 이 정확히 재발한다. **"명령을 쏘고
반환한 뒤 그 사이에 flash 코드를 실행한다"** 는 것 자체가 물리적으로 불가능하다.

그래서 우리의 비동기 단위는 **바이트가 아니라 "연산 하나(sector 1개 / page 1개)"** 다.

```
Fls_MainFunction() 한 번 호출 = flash 연산 하나를 끝까지 완료한다
                               (ITCM 에서, I-cache 를 끈 채로)
                               그 한 연산 동안에만 flash 가 busy 이고,
                               반환하는 시점에는 언제나 flash 가 idle 이다

여러 sector 지우기 = MainFunction 을 여러 번 호출하며 한 번에 한 sector 씩
```

이렇게 하면 flash 가 busy 인 구간은 오직 `Fls_MainFunction` 안쪽뿐이고, 이 함수가 반환하는
순간에는 항상 flash 가 idle 이므로 main 루프가 안전하게 flash 코드를 실행한다. 4KB 8개를 지우는
큰 job 이라도, 어느 한 `Fls_MainFunction` 호출은 40ms(1 sector)로 유계다 — N×40ms 를 한 번에
블로킹하지 않는다.

> **이것이 정직한 설계다.** 단일 flash XIP 에서 "완전한 비동기"는 코드를 전부 RAM 에 올려야만
> 가능하고, 그것은 이 챕터의 범위를 벗어난다. 대신 **"job 은 비동기, 연산 단위는 블로킹"** 이라는
> 실무적 타협을 배운다. 실제 많은 MCAL Fls 구현이 이렇게 동작한다.

---

## Part 4. 상태 머신 — 요청 / 진행 / 조회

Fls 의 심장은 하나의 내부 상태 구조체와, 그것을 다루는 세 종류의 함수다.

```
        ┌──────────────┐  Fls_Erase/Write/Read (검증 통과)  ┌──────────────┐
        │ MEMIF_IDLE   │ ─────────────────────────────────▶ │ MEMIF_BUSY   │
        │ JOB_OK       │                                    │ JOB_PENDING  │
        └──────────────┘ ◀───────────────────────────────── └──────┬───────┘
              ▲            남은 조각 0 → JOB_OK                     │
              │            하드웨어 실패 → JOB_FAILED               │ Fls_MainFunction()
              └───────────────────────────────────────────────────┘ 한 번에 한 연산
```

### 요청 함수 (`Fls_Erase` / `Fls_Write` / `Fls_Read`)

세 함수 모두 골격이 같다. **검증 → job 기록 → BUSY/PENDING 전환 → 즉시 반환** 이다.

```c
Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length)
{
    /* 1) 검증: 초기화 여부, 이미 진행 중인 job, 정렬, 허용 영역 (모두 DET 신고) */
    ...
    /* 2) job 을 내부 상태에 기록한다 */
    g_fls.job = JOB_ERASE; g_fls.addr = TargetAddress; g_fls.remaining = Length;
    g_fls.status = MEMIF_BUSY; g_fls.result = MEMIF_JOB_PENDING;
    return E_OK;   /* 요청만 접수하고 곧바로 반환한다. 실제 작업은 MainFunction 이 한다 */
}
```

### 진행 함수 (`Fls_MainFunction`)

한 번 호출될 때마다 남은 job 에서 **연산 하나** 를 처리하고, 다 끝나면 IDLE/JOB_OK 로 돌아간다.
하드웨어 연산은 F-4 프리미티브를 그대로 호출하므로, I-cache 격리와 ITCM 실행은 그 안에서 이미
처리된다.

```c
void Fls_MainFunction(void)
{
    if (g_fls.status != MEMIF_BUSY) { return; }   /* 진행할 job 이 없다 */

    switch (g_fls.job)
    {
    case JOB_ERASE:  Fls_EraseSector(...);   addr += sector; remaining -= sector; break;
    case JOB_WRITE:  Fls_ProgramPage(...);   addr += chunk;  remaining -= chunk;  break;
    case JOB_READ:   FlexSPI_ReadData(...);  addr += chunk;  remaining -= chunk;  break;
    }

    if (remaining == 0) { status = MEMIF_IDLE; result = MEMIF_JOB_OK; }
}
```

### 조회 함수 (`Fls_GetStatus` / `Fls_GetJobResult`)

내부 상태를 그대로 돌려준다. 상위 스택은 이 두 함수로 "지금 한가한가", "마지막 job 이 끝났는가"
를 판단한다.

---

## Part 5. MemIf 타입과 config 기반 영역 보호

### 왜 SPI 는 SPI_SEQ_*, Fls 는 MEMIF_* 인가

Spi 같은 통신 드라이버는 `SPI_SEQ_OK/PENDING/FAILED` 를 쓰지만, Fls·Fee 같은 **메모리 스택**은
공용 타입 `MemIf` 를 쓴다. Fee 가 Fls 위에 얹혀 **같은 상태 코드를 봐야 하기 때문** 이다. 그래서
상태는 `MEMIF_IDLE/BUSY`, 결과는 `MEMIF_JOB_OK/PENDING/FAILED` 로 통일한다.

### 하드코딩을 config 로 — 이미지 영역 보호

F-4 에서 코드에 박아 두었던 실험 영역(`FLS_WRITE_AREA_BASE/LIMIT`)을 **post-build config
테이블** 로 옮긴다. peripheral 챕터의 그 패턴이다 — 코드는 그대로 두고 config 만 바꿔 허용 영역을
조정한다.

```c
const Fls_ConfigType Fls_Config =
{
    .baseAddress   = 0x00700000u,   /* flash 뒤쪽 1MB 만 허용한다 */
    .totalSize     = 0x00100000u,
    .sectorSize    = 4096u,
    .pageSize      = 256u,
    .maxWriteChunk = 32u,
};
```

우리 이미지는 `0x0` 부터 시작하므로 이 허용 영역 밖이다. 따라서 이미지 영역을 지우거나 쓰려는
요청은 **DET 로 거부** 된다. 이것이 self-programming 의 안전장치를 코드가 아니라 **설정** 으로
표현한 것이다.

### DET — 개발 단계 오류 신고

잘못된 인자(정렬 위반, 허용 영역 밖, NULL 포인터, 초기화 전 호출 등)는 `Det_ReportError()` 로
신고하고 `E_NOT_OK` 를 반환한다. 실차에서는 로깅이나 트랩으로 처리하지만, 여기서는 UART 로
찍어 눈으로 확인한다.

| 필드 | 값(예) | 뜻 |
|---|---|---|
| module | `0x5C` (=92) | Fls 모듈 ID |
| sid | `0x01` | 서비스 ID (여기서는 Fls_Erase) |
| error | `0x03` | FLS_E_PARAM_ADDRESS (허용 영역 밖) |

---

## Part 6. 계층 분리 — facade 는 상태만, 하드웨어는 재사용

이 설계의 핵심 미덕은 **관심사의 분리** 다.

- **Fls (facade)** 는 상태 전이만 담당한다. 어떤 job 을, 어디까지, 어떤 순서로 처리할지를 안다.
  하지만 FlexSPI 레지스터나 I-cache, ITCM 은 전혀 모른다.
- **flexspi_ip (F-4 프리미티브)** 는 하드웨어만 담당한다. WREN, LUT, WIP 폴링, I-cache 격리,
  ITCM 실행을 안다. 하지만 job 이나 상태 머신은 모른다.

그래서 `Fls_MainFunction` 의 각 case 는 F-4 함수를 한 줄 호출하는 것으로 끝난다. self-programming
의 위험한 부분(I-cache off, 인터럽트 차단, ITCM 실행)은 전부 F-4 프리미티브 안에 봉인되어 있고,
facade 는 그 위에서 순수하게 "다음에 무엇을 할지" 만 결정한다.

---

## Part 7. 실측 (F-5, 2026-07-26)

```
[FLS] === F-5 MCAL Fls facade (비동기 job) ===
[FLS]   Fls_Erase req : E_OK
[FLS]   erase result : MEMIF_JOB_OK
[FLS]   Fls_Write req : E_OK
[FLS]   write result : MEMIF_JOB_OK
[FLS]   Fls_Read req  : E_OK
[FLS]   read == write : OK
[FLS]   -- DET test: 이미지 영역(0x0) erase 요청 --
[DET] mod=0x0000005C sid=0x00000001 err=0x00000003
[FLS]   Fls_Erase(0x0): E_NOT_OK
[FLS] beat 0x00000000 ...
```

- `req : E_OK` 뒤에 `result : MEMIF_JOB_OK` 가 나오는 것은, 요청이 즉시 접수되고
  `Fls_MainFunction` 폴링을 거쳐 완료됨을 뜻한다 (비동기 job 모델이 동작한다).
- `read == write : OK` 는 write→read 데이터 왕복이 일치함을 뜻한다.
- `[DET] ... err=0x03` 과 `E_NOT_OK` 는 이미지 영역 요청이 config 기반으로 거부됨을 뜻한다.
- `beat` 가 계속 이어지는 것은, MainFunction 이 연산 단위로 유계라 XIP 가 멈추지 않음을 뜻한다.

---

## 참고

- FlexSPI IP 명령 절차와 LUT 구조: [FLEXSPI_NOTES.md](FLEXSPI_NOTES.md)
- Status 레지스터 비트 의미: [NOR_STATUS_REGISTER.md](NOR_STATUS_REGISTER.md)
- 하드웨어 계층에서 겪은 트러블슈팅: [TROUBLESHOOTING_FLEXSPI_IP.md](TROUBLESHOOTING_FLEXSPI_IP.md)
