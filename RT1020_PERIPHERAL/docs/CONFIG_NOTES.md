# Post-build Config 패턴 정리 — AUTOSAR MCAL 설정 구조

> 참조: AUTOSAR CP R23-11 `SWS_BSWGeneral`(설정 클래스) + 각 드라이버 SWS 10장 "Configuration specification"
> 한 줄 요약: **"드라이버 코드와 설정 값을 분리한다 — 코드는 못 바꿔도 설정은 바꿀 수 있게."**
> 함께 진행: **D. `Mcu_GetResetReason`** (SRC_SRSR 읽기의 제자리 찾기)

---

## Part 1. 왜 Config 패턴인가

### 문제: 지금 우리 드라이버는 "값이 코드에 박제"되어 있다

지금 각 모듈의 실제 하드코딩 현황 (이번 단계에서 옮길 대상들):

| 모듈 | 코드에 박힌 값 | 의미 |
|---|---|---|
| Spi | `CCR = 130` | SCK ≈ 1MHz |
| Adc | `CFG = 0x98`, 채널 `0x19` | 12-bit·저전력, VREFSH 그룹 |
| Pwm | `VAL1 = 0xFFFF`, `VAL3 = 0x8000` | 주기, 초기 duty 50% |
| Can | `CTRL1 = (63<<24)\|...`, `LPB=1` | 비트타이밍, loopback |
| Gpt | `PR = 23`, `OCR1 = 1000000` | ÷24, 주기 1초 |
| Wdg | `Wdg1_Init(0xFFFFu)` | 타임아웃 ≈ 2.048초 |
| Icu | `COMP1 = 0x0FFF`, `PCS ÷8` | ch0 사각파 주기 |

SCK 를 2MHz 로 바꾸려면? **lpspi.c 를 열어 고치고 재컴파일.** 즉 "설정 변경 = 드라이버 코드 수정"입니다.
취미 프로젝트에선 문제없지만, 양산 관점에선 치명적입니다:

- 드라이버 코드는 **검증(테스트/인증)이 끝난 산출물** — 한 글자만 바꿔도 재검증 대상
- 같은 드라이버가 **차종/보드마다 다른 값**으로 돌아야 함 (보드레이트, 주기, 타임아웃...)
- 설정을 바꿀 사람(통합 엔지니어)과 드라이버를 짠 사람(BSW 벤더)이 **다른 회사**

### AUTOSAR 의 답: 값을 구조체로 빼고, Init 은 포인터만 받는다

```c
/* 드라이버 코드 (벤더 산출물, 불변) */
void Gpt_Init(const Gpt_ConfigType *ConfigPtr);

/* 설정 (통합 엔지니어 산출물, 프로젝트마다 다름) */
const Gpt_ConfigType Gpt_Config = { .prescaler = 23u, .period = 1000000u };

/* 응용 */
Gpt_Init(&Gpt_Config);
```

**이게 모든 MCAL `Init` 이 `const Xxx_ConfigType*` 를 받는 이유입니다.**
우리가 지금까지 `(void)ConfigPtr;` 로 버려온 그 인자가, 사실은 MCAL 구조의 심장입니다.
실무에선 이 구조체를 사람이 안 짜고 **설정 도구(EB tresos, Vector DaVinci)가 생성**합니다 —
도구 GUI 에서 "SPI 1MHz" 를 클릭하면 `Spi_Cfg.c` 가 생성되는 것.

### 설정 3계급 (Configuration Classes) — 스펙이 나누는 "언제 값이 확정되나"

| 클래스 | 값 확정 시점 | 구현 형태 | 바꾸려면 |
|---|---|---|---|
| **Pre-compile** | 컴파일 때 | `#define` | 재컴파일 |
| **Link-time** | 링크 때 | 별도 .c 의 `const` | 그 .c 만 재컴파일+재링크 |
| **Post-build** | **flash 후** | flash 특정 주소의 구조체 | **재컴파일 없이 데이터만 교체** |

Post-build 가 끝판왕입니다: ECU 를 조립한 뒤에도 설정 영역만 다시 구워 동작을 바꿉니다.
`Init(ConfigPtr)` 의 포인터 간접참조가 바로 이걸 가능하게 하는 장치 —
**코드는 "포인터가 가리키는 곳"만 알면 되니, 그 내용물은 나중에 갈아끼울 수 있습니다.**

이미 우리 프로젝트에 전례가 있습니다: `XXX_DEV_ERROR_DETECT`(DET 스위치)가 Pre-compile 클래스,
그리고 SDRAM_OTA_AB 의 "A/B 슬롯 메타데이터를 flash 에서 읽어 동작 결정"이 Post-build 와 같은 사상입니다.

---

## Part 2. 스펙 구조

### 2.1 각 SWS 10장이 정의하는 것 — Container 와 Parameter

각 드라이버 SWS 의 10장은 "설정 가능한 항목의 목록"입니다. 예를 들어 SWS_Gpt 의
`GptChannelConfiguration` container 에는 `GptChannelTickFrequency`, `GptChannelMode`(one-shot/continuous),
`GptNotification` 같은 parameter 가 있습니다. 도구는 이 스키마대로 GUI 를 만들고 코드를 생성합니다.

우리는 레벨1 답게 **각 모듈에서 실제로 하드코딩돼 있는 값만** 구조체 필드로 승격합니다 —
스펙 container 전체를 흉내내지 않습니다 (그건 도구의 영역).

### 2.2 표준 관례 4가지 (구현 시 그대로 따를 것)

1. **구조체는 `const`, 이름은 `Xxx_Config`** — flash(.rodata)에 놓임. XIP+I-cache 라 읽기 비용 걱정 없음.
2. **Init 은 NULL 검사부터** — `ConfigPtr == NULL_PTR` 이면 DET 신고 (`XXX_E_PARAM_POINTER` 또는
   전용 `XXX_E_PARAM_CONFIG`) 후 즉시 리턴. UNINIT 상태 유지.
3. **드라이버는 config 를 "복사"하지 않고 포인터를 저장** — `static const Xxx_ConfigType *Xxx_ConfigPtr;`
   런타임 내내 이 포인터로 참조 (post-build 사상 유지 + RAM 절약).
4. **재-Init 방지** — 이미 INITIALIZED 면 `XXX_E_ALREADY_INITIALIZED` 신고 (DET 단계의 상태변수가 여기서 재활용됨).

### 2.3 D. Mcu_GetResetReason — 제자리 찾기

지금 SRC_SRSR(리셋 원인) 읽기가 `Wdg_GetResetCause()` 에 있는데, 스펙상 리셋 원인은 **Mcu 모듈** 소관입니다
(SWS_Mcu 8장 `Mcu_GetResetReason`, Service ID 0x05). 반환 타입도 표준 enum:

```c
typedef enum {
    MCU_POWER_ON_RESET,     /* SRSR bit0 (ipp_reset_b) */
    MCU_WATCHDOG_RESET,     /* SRSR bit7 (wdog3_rst_b) */
    MCU_SW_RESET,
    MCU_RESET_UNDEFINED     /* 그 외 전부 */
} Mcu_ResetType;
```

이동 후 main 의 판별 코드가 이렇게 바뀝니다 — 비트 연산이 표준 이름으로 승격:

```c
/* 전 */  int by_wdg = (Wdg_GetResetCause() >> 7) & 1u;
/* 후 */  int by_wdg = (Mcu_GetResetReason() == MCU_WATCHDOG_RESET);
```

`Wdg_GetResetCause/ClearResetCause` 는 Wdg 에서 제거하고 Mcu 로 이사 (기능 변화 0, 소속만 정정).

---

## Part 3. 우리 프로젝트 적용 설계

### 3.1 모듈별 ConfigType 필드 (하드코딩 → 구조체 승격 목록)

| 모듈 | ConfigType 필드 | 현재 하드코딩 위치 |
|---|---|---|
| Spi | `sckdiv` (이미 필드 있음! 안 쓰고 있을 뿐) | lpspi.c `LPSPI1_CCR = 130` |
| Adc | `group0_channel` | Adc.c `#define ADC_GROUP0_CHANNEL 0x19` |
| Pwm | `period` (VAL1), `initial_duty` (표준 0x8000 스케일) | Pwm.c VAL1/VAL3 |
| Can | `presdiv`, `loopback` (비트타이밍 나머지는 파생) | Can.c CTRL1 조립식 |
| Gpt | `prescaler` (PR), `period_ticks` (OCR1) | Gpt.c 23 / 1e6 |
| Wdg | `timeout_ticks` (TOVAL), `window_ticks` (WIN, 지금 0) | Wdg.c `Wdg1_Init(0xFFFF)` |
| Icu | `gen_half_period` (ch0 COMP1) | Icu.c 0x0FFF |

원칙: **register 층 함수 시그니처가 값을 인자로 받게** 바꾸고 (예: `Gpt1_Init(uint32_t pr, uint32_t ocr1)`),
facade 가 config 필드를 꺼내 전달합니다. register 층은 여전히 AUTOSAR 를 모릅니다 (계층 순수성 유지).

### 3.2 설정 파일 배치 — "도구가 생성했다고 치는" 자리

```
src/Cfg.c        ← 7개 모듈의 const Xxx_Config 정의 전부 (통합 엔지니어의 파일)
include/Cfg.h    ← extern 선언
```

한 파일에 모으는 이유: "이 파일만이 프로젝트별 산출물이고 나머지는 불변 드라이버"라는
경계선을 물리적으로 보여주기 위해서입니다. main.c 는 `Xxx_Init(&Xxx_Config)` 로 바뀝니다.

### 3.3 주의 의존성 (보드 재검토에서 확인된 것)

- **Wdg reconfig 윈도우**: config 로 받은 TOVAL 설정 자체는 기존 unlock 시퀀스 그대로라 영향 없음.
  단 검증에서 TOVAL 을 줄여볼 때 **한 beat(≈0.26초)보다 짧게 잡으면 안 됨** (즉사 루프).
- **Gpt OCR1 변경 검증**: tick 주기가 바뀌면 "beat 당 tick 증가량"으로 확인 — 워치독 마진(2.048초)과
  무관하도록 OCR1 은 500000(0.5초) 정도까지만.
- **Spi sckdiv**: CCR 은 MEN=0 일 때만 쓸 수 있으므로 (P-1d 교훈) init 시퀀스 내 위치 유지.

### 3.4 검증 계획 (A-c) — "코드 안 바꾸고 설정만 바꿔서 동작이 바뀜"을 실증

이 단계의 합격 기준은 회귀 무결 + **설정 교체 실험**입니다:

```
① 기본 Cfg.c 로 빌드          → 기존 출력과 완전 동일 (회귀 기준선)
② Cfg.c 의 Gpt period 만 절반  → 드라이버 코드 무수정, beat 당 tick 이 약 2배로
③ Cfg.c 의 Pwm initial_duty 25% → VAL3 readback 이 0x3FFF 로
④ 되돌리고 재확인              → ① 과 동일
⑤ (negative) Xxx_Init(NULL)    → DET 신고 + UNINIT 유지 (이후 API 호출도 UNINIT 신고)
```

②③이 핵심 스토리입니다: **git diff 에 Cfg.c 한 파일만 뜨는데 보드 동작이 바뀝니다** —
"검증된 드라이버는 그대로, 설정만 프로젝트별로"의 실물 증명.

### 실측 결과 (2026-07-07, 보드 검증 완료)

Cfg.c 두 줄(Gpt `period_ticks` 1000000→500000, Icu `gen_half_period` 0x0FFF→0x07FF)만 수정:

| 지표 | 기준선 | 설정 교체 후 | 비율 |
|---|---|---|---|
| Gpt tick (beat 0x17 시점) | 0x11 | 0x22 | **2.00배** |
| Icu edge 증가량 / beat | 0x20A | 0x414 | **2.00배** |
| Gpt_GetTimeRemaining | 0x000EF7E4 | 0x000756C4 | OCR1 반토막 반영 |

드라이버 .c/.h 는 한 글자도 수정하지 않았고, 워치독 데모(POR→타임아웃 리셋→감지→생존)는
양쪽 빌드에서 동일하게 동작. NULL-Init 거부 시나리오(⑤)는 A-b5 진행 중 main 의
`Can_Init(0)` 갱신 누락 사고로 자연 실증됨 — DET 신고(api=0x00, err=0x01)만으로 진단,
Can 만 죽고 나머지 시스템은 무사(fail-safe 방향).

---

## Part 4. 면접 연결

- "MCAL Init 이 왜 전부 `const ConfigType*` 를 받는가" — post-build 설정 교체 구조를 직접 구현해 봄.
- 설정 3계급(pre-compile/link-time/post-build)과 각각의 재검증 비용 차이를 한 문장으로.
- "도구(EB tresos)가 생성하는 Cfg.c 의 정체" — 스키마(SWS 10장) → GUI → 생성 코드 흐름.
- DET 와의 합: 상태변수(UNINIT/INITIALIZED)가 NULL config 거부·재-Init 방지에 재활용됨 —
  "단계마다 쌓은 인프라가 다음 단계의 부품이 된다."
