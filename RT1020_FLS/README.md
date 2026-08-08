# RT1020 FLS — FlexSPI NOR 드라이버와 AUTOSAR Fee

> NOR flash 를 **FlexSPI IP 커맨드로 직접 제어**하고, 그 위에 AUTOSAR MCAL 의 `Fls`
> 비동기 job 모델과 `Fee`(Flash EEPROM Emulation)를 올린 프로젝트입니다. 타깃은
> Cortex-M7 기반 NXP i.MX RT1020 입니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![bus](https://img.shields.io/badge/bus-FlexSPI_IP_command-blue)]()
[![autosar](https://img.shields.io/badge/AUTOSAR-Fls_%2B_Fee-green)]()
[![storage](https://img.shields.io/badge/flash-NOR_XIP-lightgrey)]()

---

## 한눈에

XIP 로 부팅하는 시스템에서 **자기가 실행되고 있는 flash 를 자기가 지우는 것**이 이 챕터의
핵심 난제입니다. 지우는 동안 flash 는 명령을 받지 않으므로, 그 코드가 flash 에 있으면
자기 발밑을 파는 셈이 됩니다.

해법은 지우기·쓰기 코드를 **ITCM 으로 옮겨 실행**하는 것입니다. flash 가 응답하지 않는
동안에도 CPU 는 내부 메모리에서 계속 돌 수 있습니다.

그 위에 AUTOSAR 계층을 얹었습니다. `Fls` 는 **비동기 job 모델**이라 호출이 즉시 반환되고
`Fls_MainFunction()` 이 주기적으로 진행시킵니다. `Fee` 는 그 위에서 블록 단위 저장소를
흉내내며, flash 의 "덮어쓰기 불가" 성질을 append 와 가비지 컬렉션으로 감춥니다.

---

## 계층 구조

```
┌────────────────────────────────────────────┐
│ 응용 (main.c)                               │
└────────────────┬───────────────────────────┘
                 │ Fee_Write(블록번호, 데이터)
┌────────────────▼───────────────────────────┐
│ Fee — Flash EEPROM Emulation                │
│   블록 단위 read/write/invalidate           │
│   append 기록, 뱅크 전환, 가비지 컬렉션      │
└────────────────┬───────────────────────────┘
                 │ Fls_Write(주소, 버퍼, 길이)
┌────────────────▼───────────────────────────┐
│ Fls — AUTOSAR MCAL facade                   │
│   비동기 job 모델 (MainFunction 이 진행)     │
│   상태·job 결과 조회, DET 오류 보고          │
└────────────────┬───────────────────────────┘
                 │ flexspi_ip 저수준 명령
┌────────────────▼───────────────────────────┐
│ FlexSPI IP command — 레지스터 직접 제어      │
│   JEDEC ID, 상태 레지스터, WEL, 페이지 프로그램│
│   sector erase (ITCM 에서 실행)              │
└────────────────────────────────────────────┘
```

---

## API

**Fls** — AUTOSAR MCAL 규약을 따릅니다.

```c
void                Fls_Init(const Fls_ConfigType *ConfigPtr);
Std_ReturnType      Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
Std_ReturnType      Fls_Write(Fls_AddressType TargetAddress, const uint8_t *Src, Fls_LengthType Length);
Std_ReturnType      Fls_Read (Fls_AddressType SourceAddress, uint8_t *Dst, Fls_LengthType Length);
void                Fls_MainFunction(void);          /* job 을 실제로 진행시킴 */
MemIf_StatusType    Fls_GetStatus(void);
MemIf_JobResultType Fls_GetJobResult(void);
```

주소는 **flash 기준 오프셋(0-base)** 입니다. AHB 주소(`0x60xxxxxx`)가 아닙니다. 두 주소
공간을 섞는 것이 이 계층에서 가장 흔한 실수입니다.

**Fee** — 블록 단위 저장소입니다.

```c
void           Fee_Init(void);
Std_ReturnType Fee_Read(uint16_t BlockNumber, uint16_t Offset, uint8_t *Buf, uint16_t Length);
Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *Buf);
Std_ReturnType Fee_InvalidateBlock(uint16_t BlockNumber);
void           Fee_MainFunction(void);
```

---

## 왜 비동기인가

flash sector 하나를 지우는 데 수십에서 수백 밀리초가 걸립니다. 그동안 CPU 를 붙잡아 두면
제어 주기를 놓칩니다. 그래서 AUTOSAR 의 메모리 스택은 **요청과 진행을 분리**합니다.

```c
Fls_Erase(addr, len);            /* 즉시 반환. 요청만 접수 */

while (Fls_GetStatus() == MEMIF_BUSY) {
    Fls_MainFunction();          /* 주기 태스크가 조금씩 진행 */
    /* 그 사이 다른 일을 할 수 있음 */
}

if (Fls_GetJobResult() == MEMIF_JOB_OK) { ... }
```

이 구조를 직접 만들어 보면 AUTOSAR 의 `MainFunction` 규약이 왜 그렇게 생겼는지 이해가
됩니다. **상태 기계를 드라이버 안에 두고 밖에서는 주기적으로 밀어주기만 하는 것**이
핵심입니다.

## 왜 Fee 가 필요한가

NOR flash 는 **비트를 1에서 0으로만 바꿀 수 있고**, 되돌리려면 sector 단위로 지워야
합니다. 값 하나를 바꾸려고 sector 전체를 지우면 수명이 빨리 닳고, 지우는 도중 전원이
끊기면 그 sector 의 다른 데이터까지 잃습니다.

Fee 는 이 성질을 감춥니다.

- **Append**: 값을 갱신할 때 덮어쓰지 않고 뒤에 새로 씁니다. 읽을 때는 가장 최근 것을
  찾습니다.
- **뱅크 전환**: 두 뱅크를 두고 한쪽이 차면 유효한 블록만 반대편으로 옮긴 뒤 옛 뱅크를
  지웁니다. 옮기는 도중 전원이 끊겨도 옛 뱅크가 그대로 남아 있습니다.
- **무효화**: 블록을 지우는 것도 "지웠다" 는 기록을 append 하는 방식입니다.

---

## 진행 단계

| 단계 | 내용 |
|---|---|
| F-0 | 챕터 스캐폴딩 (Peripheral 챕터의 공통 코드 재사용) |
| F-1 | FlexSPI IP 커맨드로 JEDEC ID 읽기 — 통신이 되는지부터 확인 |
| F-2 | 상태 레지스터와 데이터 읽기 |
| F-3 | Write Enable Latch 제어 |
| F-4 | **ITCM 에서 실행하는 sector erase** — flash 응답 정지 구간 통과 |
| F-5 | 페이지 프로그램 + erase-before-write 시연 |
| F-6 | `Fls` MCAL facade (비동기 job 모델) |
| F-7 | `Fee` 뱅크 레이아웃, init 스캔, 블록 read |
| F-8 | `Fee` 블록 write (append) + 재부팅 후 지속성 확인 |
| F-9 | `Fee` 가비지 컬렉션 (뱅크 전환, 블록 무효화) |

---

## 문서

| 문서 | 내용 |
|---|---|
| [FLEXSPI_NOTES.md](docs/FLEXSPI_NOTES.md) | FlexSPI IP 커맨드 경로와 NOR flash 기초 |
| [NOR_STATUS_REGISTER.md](docs/NOR_STATUS_REGISTER.md) | 상태 레지스터 비트, WEL 동작, erase 중 타이밍 실측 |
| [FLS_FACADE.md](docs/FLS_FACADE.md) | MCAL Fls facade 와 비동기 job 모델 설계 |
| [TROUBLESHOOTING_FLEXSPI_IP.md](docs/TROUBLESHOOTING_FLEXSPI_IP.md) | IP 커맨드·페이지 프로그램에서 막혔던 사례들 |

---

## 사용법

```bash
./build.sh
./flash_mcu.sh
```

UART 로 각 단계의 결과가 출력됩니다.

---

## 겪은 문제들

**erase 중 WEL 이 언제 내려가는가** — 데이터시트만으로는 판단이 안 되어 상태 레지스터를
폴링하며 직접 측정했습니다. 그 결과를 문서에 기록해 두었습니다. **하드웨어 동작은
추측하지 말고 관측해야 합니다.**

**flash 가 응답하지 않는 동안의 코드 위치** — erase 루틴이 flash 에 있으면 자기 자신을
읽을 수 없습니다. `.ramfunc` 로 ITCM 에 배치해 해결했습니다.

**Hard Fault 원인 추적** — 스택에 쌓인 PC 와 LR 을 꺼내 출력하는 핸들러를 만들어,
어디서 죽었는지 주소로 좁혔습니다.

---

## 이어지는 챕터

이 챕터의 flash 드라이버를 그대로 재사용해 메모리 신뢰성을 다룬 것이
[RT1020_ECC](../RT1020_ECC/) 입니다. AUTOSAR 계층 설계는
[RT1020_PERIPHERAL](../RT1020_PERIPHERAL/) 에서 이어집니다.

---

_학습 및 포트폴리오 목적입니다._
