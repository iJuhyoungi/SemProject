# WdgM (Watchdog Manager) 정리 — AUTOSAR SWS 기준

> 참조: AUTOSAR CP R23-11 `SWS_WatchdogManager` (BSW System Services — MCAL 위층)
> 한 줄 요약: **"워치독을 '타이머 리셋'에서 '프로그램 실행 흐름의 논리 감시'로 승격시키는 관리자."**
> 이 프로젝트 AUTOSAR 확장 코스의 피날레 — DET(신고 인프라) + Config(설정 인프라) + Gpt tick + RTWDOG 를 전부 조립.

---

## Part 1. 왜 WdgM 인가

### 문제: 지금 우리 refresh 는 "형식적"이다

현재 구조를 보면 main 루프가 워치독을 **직접** 먹입니다:

```c
while (1) {
    Wdg_SetTriggerCondition(feed ? 100u : 0u);   /* 루프에 왔으니 일단 먹임 */
    ... 작업 ...
}
```

이 감시가 잡는 것: "CPU 가 완전히 멈췄다" (hang, 무한 fault 루프).
이 감시가 **못 잡는 것**: "코드가 돌긴 도는데 **잘못** 돈다."

- 포인터가 깨져 main 루프의 **일부만** 무한 반복 → refresh 줄이 그 안에 있으면 **영원히 생존**
- 인터럽트 폭주로 main 이 기어가는데 → 어쩌다 한 번씩은 refresh 줄에 도달 → 생존
- 로직 오류로 루프가 **너무 빨리** 공회전 (해야 할 일을 건너뜀) → refresh 는 더 자주 → 생존

즉 "refresh 명령 실행됨" ≠ "프로그램이 건강함". P-11 때 배운 매직값·window 는
**HW 레벨** 방어였고, 이제 **SW 실행 흐름 레벨** 방어를 얹을 차례입니다.

### AUTOSAR 의 답: 감시를 간접화(indirect)한다

```
[지금]  main ──직접 refresh──▶ RTWDOG

[WdgM]  main ──"체크포인트 지났음" 보고──▶ WdgM ──판정: 보고 횟수가          ──▶ Wdg_SetTriggerCondition
        (Supervised Entity)               정상 범위인가? (너무 적어도,
                                          너무 많아도 FAIL)
```

응용 코드는 워치독을 **만질 수 없게 되고**, 대신 "나 여기 지나갔다"는 **증언**만 합니다.
WdgM 이 증언들을 모아 "실행 흐름이 논리적으로 정상인가"를 판정하고, 정상일 때만 먹입니다.
**"너무 안 돌아도 죽고(hang), 너무 자주 돌아도 죽는다(폭주)"** — 이게 핵심 승격입니다.

---

## Part 2. 스펙 구조

### 2.1 용어 3개 (이것만 잡으면 스펙이 읽힌다)

| 용어 | 뜻 | 우리 프로젝트에선 |
|---|---|---|
| **Supervised Entity (SE)** | 감시 대상인 논리적 코드 단위 | main beat 루프 (SE 1개) |
| **Checkpoint** | SE 안의 "지나갔음을 보고하는 지점" | 루프 내 보고 지점 |
| **Alive Supervision** | "기준 주기 안에 체크포인트 도달 횟수가 min~max 범위인가" 검사 | GPT tick 마다 판정 |

(스펙엔 Deadline/Logical supervision 도 있지만 — 체크포인트 간 시간/순서 검사 — 레벨1 은 Alive 만.)

### 2.2 API (SWS 8장)

```c
void WdgM_Init(const WdgM_ConfigType *ConfigPtr);                     /* SID 0x00 */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID,
                                      WdgM_CheckpointIdType CheckpointID);  /* SID 0x0E — SE가 부름 */
void WdgM_MainFunction(void);                                         /* SID 0x08 — 주기 판정 */
Std_ReturnType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID,
                                   WdgM_LocalStatusType *Status);     /* SID 0x0C */
Std_ReturnType WdgM_GetGlobalStatus(WdgM_GlobalStatusType *Status);   /* SID 0x0D */
```

동작 흐름:
```
SE:            WdgM_CheckpointReached() ──▶ alive counter++      (가볍다: 카운터 1증가)
주기(tick):    WdgM_MainFunction() ──▶ counter 를 min~max 와 비교
                 ├─ 범위 안  → status OK     → Wdg_SetTriggerCondition(timeout>0)  먹임
                 └─ 범위 밖  → FAILED/EXPIRED → Wdg_SetTriggerCondition(0)          굶김 → HW 리셋
               counter = 0 (다음 창 시작)
```

### 2.3 상태 머신 (단순화)

```
LOCAL:  OK ──(창 위반, 허용횟수 남음)──▶ FAILED ──(회복)──▶ OK
              └──(허용횟수 소진)──▶ EXPIRED (복귀 없음 → 굶겨 죽임)
GLOBAL: 모든 SE OK → OK / 하나라도 FAILED → FAILED / 하나라도 EXPIRED → EXPIRED
```

`FailedAliveSupervisionRefCycleTol` = "몇 번의 창 위반까지 봐줄 것인가" (일시적 지터 허용).
레벨1 은 tolerance 1 (한 번 봐주고 두 번째 위반에 EXPIRED) 정도가 적당.

### 2.4 설정 파라미터 (SWS 10장 → 우리 Config 패턴에 그대로 얹힘)

| SWS 파라미터 | 뜻 | 우리 값 (산정 근거는 Part 3) |
|---|---|---|
| SupervisionReferenceCycle | 판정 주기 (MainFunction 몇 회마다 판정) | 1 (= GPT tick ≈0.4초마다) |
| ExpectedAliveIndications + Min/MaxMargin | 창당 기대 보고 횟수 범위 | min 1 / max 6 |
| FailedAliveSupervisionRefCycleTol | 위반 허용 횟수 | 1 |

WdgM 모듈 ID = **13**. 에러코드는 `WDGM_E_NO_INIT`/`WDGM_E_PARAM_SEID` 등 —
구현 시 SWS 7장 표에서 베낀다 (DET 코스 원칙 그대로).

---

## Part 3. 우리 프로젝트 적용 설계 (보드 실측 수치 기반)

### 3.1 타이밍 산정 — 전부 실측에서 나온 숫자

```
GPT tick(판정 주기)     ≈ 0.4초     (period_ticks=1e6. C-c 실측 역산 — GPT 소스 ipg 가
                                    "24MHz÷24=1MHz" 가정과 달리 ~60MHz 대 = ROM 기본 클럭)
RTWDOG TOVAL           = 2.048초    (Wdg_Config.timeout_ticks = 0xFFFF)
main beat              ≈ 0.29초     (실측 비율: beat당 tick 0.72)
체크포인트 보고         = 루프당 2회  (기존 refresh 2지점을 보고 지점으로 전환)
→ 판정 창당 기대 보고   ≈ 2.7회     (2회 / 0.72 tick)
```

**판정 마진**: 판정+먹임이 ≈0.4초마다 → TOVAL 2.048초 → **판정 4~5회를 통째로 걸러도
안 죽는 여유** (굶김 결정은 tolerance 로 명시적으로만). **min=1 / max=6**: 지터 감안 넉넉히,
그래도 "보고 0회(hang)"와 "보고 7회+(폭주)"는 확실히 잡는 창.

> tick 주기의 진실: 처음 "1초"로 가정하고 리셋을 beat 0x9 로 예측했으나 실측은 beat 0xD —
> 굶김→리셋 간격이 정확히 TOVAL(2.048초)이라는 사실로부터 tick≈0.4초를 역산해 확정.
> "예측이 어긋나면 모델이 틀린 것" — 가정(클럭 트리)을 실측으로 교정한 사례.

### 3.2 구조 배치 — 어디서 뭘 부르나

| 코드 | 컨텍스트 | 역할 |
|---|---|---|
| `WdgM_CheckpointReached(0,0)` | main 루프 (기존 refresh 2지점) | 증언만. Wdg 직접 호출 **삭제** |
| `WdgM_MainFunction()` | **GPT1 tick ISR** (≈0.4초) | 판정 + 먹임/굶김. **UART 금지** (DET 규칙) |
| `WdgM_GetGlobalStatus()` | main 루프 | 상태를 UART 로 출력 (관측용) |

**refresh 창구 단일화가 안전의 전제**: main 의 `Wdg_SetTriggerCondition` 직접 호출이
남아 있으면 WdgM 판정이 무의미해짐 (죽여야 할 때 main 이 살려버림). 전환 시 전부 제거.

ISR 규칙 (DET 코스에서 명문화한 것): tick ISR 안에서는 카운터 비교 + RTWDOG refresh
write 1회뿐 (수십 사이클). Det/UART 는 절대 금지 — 상태 플래그만 세우고 main 이 출력.

### 3.3 파일 구조

```
include/WdgM.h    ← 타입(SEID/status/ConfigType) + API + MODULE_ID/SID/에러코드
src/WdgM.c        ← alive counter, 상태머신, MainFunction
src/Cfg.c         ← WdgM_Config (expected min/max, tolerance) 추가
src/Gpt.c         ← tick ISR 에서 WdgM_MainFunction() 호출 (한 줄)
src/main.c        ← refresh 2지점 → CheckpointReached, 데모 조건 재작성
```

계층 주의: WdgM 은 MCAL 이 아니라 **BSW 서비스층** — Wdg **위에** 앉아 표준 Wdg API
(`Wdg_SetTriggerCondition`)만 부릅니다. RTWDOG 레지스터를 직접 만지지 않음 (계층 순수성).

### 3.4 데모 재설계 — 서사가 좋아진다

```
[기존] beat<5 동안 main 이 직접 먹임 → 5부터 굶김 → 리셋
[신규] beat<5 동안 체크포인트 보고 → 5부터 "보고 중단" (SE 가 침묵)
       → WdgM 이 다음 판정 창에서 위반 감지 → FAILED (tolerance 1회 소진)
       → 그 다음 창도 위반 → EXPIRED → 굶김 → TOVAL 후 HW 리셋
       → 재부팅: MCU_WATCHDOG_RESET 감지 → 계속 보고 → 영구 생존
```

실측 (C-c, 2026-07-08): status `0x00`(beat 0~5) → `0x01` FAILED(6~7, tolerance 1회 소진)
→ `0x02` EXPIRED(8~D, 굶김) → beat 0xD 이후 리셋 → 재부팅 SRSR=0x80 감지 → 영구 생존.
UART 로 `OK → FAILED → EXPIRED` 전이가 보이는 게 기존 데모와의 결정적 차이.

폭주 실험 실측: 한 창에 22회 몰아 보고(burst 20+정상 2) → 다음 beat 에 `0x01` FAILED
(22 > max 6) → 그 다음 창에 `0x00` 회복(tolerance). **"너무 자주 돌아도 잡는다"** 실증 —
직접 먹이기 방식으론 원리적으로 불가능한 검출. 1차 부팅에선 폭주 FAILED(beat 4)와
hang FAILED→EXPIRED(beat 6~)가 한 화면에 연달아 찍혀 두 고장 모드가 동시 시연됨.

### 3.5 검증 계획 (C-c)

```
① 정상 경로: 보고 지속 → status OK 유지 + 영구 생존 (2차 부팅과 동일 조건)
② hang 시뮬: beat 5부터 보고 중단 → OK→FAILED→EXPIRED 전이 로그 → 리셋 → SRSR 0x80
③ 재부팅 감지 후 영구 생존 (①로 회귀)
④ 폭주 시뮬 ⭐: 한 beat 에서 CheckpointReached 를 20회 몰아 호출
   → max(6) 초과 → FAILED — "너무 자주 돌아도 잡는다"의 실증. 직접 먹이기 방식으론
   원리적으로 불가능했던 검출이라, 이 실험이 WdgM 의 존재 증명.
⑤ 회귀: 나머지 6개 드라이버 데모 무결
```

### 3.6 보너스 (C 완료 후 선택): HW window mode 와 이중화

Wdg_Config.window_ticks 를 살리고 CS[WIN] 을 켜면 "너무 이른 refresh" 를 **HW 도** 거부.
SW(WdgM max 검사) + HW(window) 이중 방어 — UPDATE=1 이라 재설정 가능함은 A-b2 에서 확인됨.

---

## Part 4. 면접 연결

- **기능안전 서사의 완성**: "refresh 는 형식이 아니라 실행 흐름이 논리적으로 정상이라는 증명이어야
  한다. 그래서 AUTOSAR 는 응용이 워치독을 직접 못 만지게 하고 WdgM 이 간접 감시한다."
- ISO 26262 연결: alive/deadline/logical supervision = 표준이 요구하는 SW 실행 흐름 감시
  메커니즘의 구현체. "폭주도 잡는다(max 검사)"가 직접 먹이기와의 결정적 차이.
- 코스 전체 회수: DET(신고)·Config(설정 테이블)·Gpt tick(판정 주기)·Wdg(액추에이터)가
  전부 WdgM 의 부품으로 재사용됨 — "단계마다 쌓은 인프라가 다음 단계의 부품이 된다"의 종착점.
