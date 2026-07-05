# DET (Default Error Tracer) 정리 — AUTOSAR SWS 기준

> 참조: AUTOSAR CP R23-11 `SWS_DefaultErrorTracer` + 각 드라이버 SWS 의 7장 "Error classification"
> 한 줄 요약: **"모든 BSW 모듈이 'API 를 잘못 쓴 개발자 실수'를 한 곳으로 신고하는 공용 신고 창구."**

---

## Part 1. 왜 DET 인가

### 문제: 지금 우리 facade 는 "조용히 잘못 동작"한다

```c
Adc_ReadGroup(7, buf);      /* 그룹 7 은 없음 — 그런데 지금 코드는 그냥 group 무시하고 진행 */
Pwm_SetDutyCycle(0, duty);  /* Pwm_Init 안 불렀어도 레지스터에 그냥 씀 — 무슨 일이 일어날지 모름 */
```

베어메탈에서 이런 호출은 **에러도 안 나고, 크래시도 안 나고, 그냥 이상하게 동작**합니다.
P-5 에서 겪은 `Spi_WriteIB` 의 `s_ib_len` 누락 버그가 정확히 이 유형이었습니다 —
guard 가 **조용히** early-return 해서 반나절을 태웠죠. "조용한 실패"가 베어메탈 디버깅의 최대 비용입니다.

### AUTOSAR 의 답: 개발 에러는 전부 DET 로 몰아라

AUTOSAR 는 에러를 두 종류로 나눕니다. 이 구분이 DET 이해의 절반입니다.

| 구분 | Development Error | Runtime/Production Error |
|---|---|---|
| 원인 | **개발자가 API 를 잘못 씀** (계약 위반) | 하드웨어/환경 (버스 에러, 타임아웃) |
| 예 | init 전에 호출, 없는 채널, NULL 포인터 | CAN bus-off, ADC 변환 실패 |
| 보고처 | **DET** (Det_ReportError) | DEM (진단 이벤트 → 정비소 OBD) |
| 양산 빌드 | **컴파일 스위치로 통째 제거 가능** | 항상 유지 |

핵심 통찰: development error 는 **코드가 올바르면 절대 발생하지 않는** 에러입니다.
그래서 개발 중에만 켜고(`DevErrorDetect=true`), 검증 끝난 양산 빌드에선 0 바이트로 사라집니다.
— "디버그 assert 의 표준화·전사화" 라고 이해하면 정확합니다.

---

## Part 2. 스펙 구조

### 2.1 API — 딱 하나만 알면 된다

```c
Std_ReturnType Det_ReportError(
    uint16 ModuleId,     /* 누가        — 신고한 모듈 (AUTOSAR 표준 모듈 번호) */
    uint8  InstanceId,   /* 몇 번째     — 같은 모듈 인스턴스 구분 (우리는 0 고정) */
    uint8  ApiId,        /* 어느 함수   — 모듈 SWS 의 Service ID */
    uint8  ErrorId       /* 무슨 잘못   — 모듈 SWS 의 에러 코드 */
);
```

4개 인자가 곧 "신고서 양식"입니다: **누가 / 몇 번째 / 어느 함수에서 / 무슨 잘못**.
DET 자체는 이 신고를 받아 **기록·전달만** 합니다 (복구는 호출한 모듈 책임 — DET 는 로거).

### 2.2 ModuleId — AUTOSAR 표준 모듈 번호 (TR_BSWModuleList)

| 모듈 | Id | 모듈 | Id |
|---|---|---|---|
| Can | 80 | Dio | 120 |
| Spi | 83 | Pwm | 121 |
| Gpt | 100 | Icu | 122 |
| Mcu | 101 | Adc | 123 |
| Wdg | 102 | Port | 124 |

(Det 자신은 15. 우리 7개 모듈이 전부 표준 번호를 갖고 있음 — 이 번호로 헤더에 `#define SPI_MODULE_ID 83` 식으로 박는다.)

### 2.3 ApiId — 각 SWS 가 함수마다 부여한 Service ID (예: Spi)

| API | Service ID |
|---|---|
| Spi_Init | 0x00 |
| Spi_WriteIB | 0x02 |
| Spi_AsyncTransmit | 0x03 |
| Spi_SyncTransmit | 0x04 |

(각 SWS 8장의 함수 정의표에 "Service ID [hex]" 로 명시. 우리 헤더에 `SPI_SID_INIT` 식으로 정의.)

### 2.4 ErrorId — 각 SWS 7장의 에러 분류표 (대표 패턴 3가지)

모듈마다 코드값은 다르지만 **패턴은 셋뿐**입니다:

| 패턴 | 예 (이름은 SWS 표준) | 뜻 |
|---|---|---|
| **UNINIT** | `SPI_E_UNINIT`, `ADC_E_UNINIT`, `GPT_E_UNINIT` | init 전에 다른 API 호출 |
| **PARAM_xxx** | `ADC_E_PARAM_GROUP`, `PWM_E_PARAM_CHANNEL`, `SPI_E_PARAM_POINTER` | 잘못된 채널/그룹/NULL |
| **상태 위반** | `SPI_E_SEQ_PENDING`, `PWM_E_PERIOD_UNCHANGEABLE`, `WDG_E_PARAM_MODE` | 지금 상태에선 그 호출 불가 |

> 정확한 hex 값은 각 모듈 SWS 7장 표 기준으로 구현 시 하나씩 옮긴다. (예: SPI_E_UNINIT=0x1A,
> ADC_E_UNINIT=0x0A, PWM_E_UNINIT=0x11 — 모듈마다 다르므로 "외우지 말고 SWS 를 베낀다".)

### 2.5 실무 MCAL 코드의 표준 모양 (이게 이번 확장의 목표 형태)

```c
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr)
{
#if (ADC_DEV_ERROR_DETECT == STD_ON)                        /* ← 양산 빌드에선 통째 제거 */
    if (Adc_State == ADC_UNINIT) {
        Det_ReportError(ADC_MODULE_ID, 0, ADC_SID_READGROUP, ADC_E_UNINIT);
        return E_NOT_OK;
    }
    if (Group >= ADC_MAX_GROUP) {
        Det_ReportError(ADC_MODULE_ID, 0, ADC_SID_READGROUP, ADC_E_PARAM_GROUP);
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL_PTR) {
        Det_ReportError(ADC_MODULE_ID, 0, ADC_SID_READGROUP, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }
#endif
    /* ... 여기부터 실제 동작 ... */
}
```

**모든 API 의 첫 블록이 이 모양**입니다. 실무 MCAL 소스를 열면 절반이 이 guard 입니다 —
이걸 직접 짜 보면 "왜 MCAL 코드가 그렇게 생겼는지"를 몸으로 알게 됩니다.

---

## Part 3. 우리 프로젝트 적용 설계

### 3.1 구조 — 신규 2파일 + 기존 facade 에 guard 삽입

```
include/Det.h       ← Det_ReportError 선언 + DET_ENABLED 스위치
src/Det.c           ← 구현: UART 로 신고 내용 출력 (우리의 "tracer 백엔드")
src/Adc.c 등 7개    ← 각 facade API 입구에 guard 블록 추가
include/Adc.h 등    ← MODULE_ID / SID / E_xxx 상수 + 상태 enum 추가
```

우리 Det.c 백엔드는 UART 한 줄 출력:

```
[Det] mod=123 api=0x04 err=0x0A   ← Adc_ReadGroup 이 UNINIT 신고
```

### 3.2 모듈 상태 변수 — UNINIT 체크의 전제

UNINIT 을 잡으려면 각 모듈이 **자기 상태를 기억**해야 합니다:

```c
static Adc_StateType Adc_State = ADC_UNINIT;   /* Adc_Init() 에서 ADC_IDLE 로 */
```

이 상태 변수는 다음 단계(A. Config 패턴)에서 `ALREADY_INITIALIZED` 체크와
상태머신의 기반이 되므로, DET 단계에서 미리 심는 것이 설계상 이득입니다.

### 3.3 어디에 어떤 guard 를 넣나 (모듈별 최소 셋)

| 모듈 | UNINIT 체크 대상 API | PARAM 체크 |
|---|---|---|
| Spi | WriteIB / SyncTransmit / AsyncTransmit | Channel==0, 포인터 NULL, len==0 |
| Adc | ReadGroup / GetGroupStatus | Group==0, 포인터 NULL |
| Pwm | SetDutyCycle | Channel==0, duty ≤ 0x8000 |
| Can | SetControllerMode / Write | Hth==0, PduInfo NULL, length ≤ 8 |
| Gpt | GetTimeElapsed / Remaining / Notification | Channel==0 |
| Wdg | SetMode / SetTriggerCondition | (SetMode 는 기존 OFF-reject 유지) |
| Icu | GetEdgeNumbers / ResetEdgeCount | Channel==0 |

우리 채널/그룹이 전부 단일(0)이라 PARAM 체크는 `!= 0` 이면 신고 — 단순하지만 스펙 모양 그대로.

### 3.4 설계 규칙 2개 (보드 의존성에서 도출)

1. **ISR 컨텍스트에서 Det → UART 직행 금지.** 우리 UART 는 blocking polled 라 ISR 이 밀린다.
   지금 guard 대상 API 는 전부 main 컨텍스트라 문제없지만, 규칙으로 명문화 (WdgM 때 중요해짐).
2. **`DET_ENABLED` 컴파일 스위치 필수.** "양산 빌드에선 사라진다"는 컨셉 자체가 학습 포인트 —
   `#if` 로 감싸고, 꺼도 빌드·동작이 동일함을 확인하는 것까지가 검증.

### 3.5 검증 계획 (B-c) — 일부러 잘못 호출해서 신고 받기

main.c 데모에 **negative test 블록** 추가:

```
① Adc_Init 전에 Adc_ReadGroup 호출   → [Det] mod=123 ... err=UNINIT 기대
② Adc_ReadGroup(7, buf)              → [Det] ... err=PARAM_GROUP 기대
③ Adc_ReadGroup(0, NULL)             → [Det] ... err=PARAM_POINTER 기대
④ 정상 호출                          → Det 침묵 + 기존 결과 그대로 (회귀 없음)
⑤ DET_ENABLED=0 재빌드               → ①~③ 이 침묵, 기존 데모 전부 정상
```

합격 기준: ①~③ 에서 정확한 mod/api/err 값 출력 + ④⑤ 회귀 없음.

---

## Part 4. 면접 연결

- "실무 MCAL 코드의 절반이 DET guard 인 이유" — development vs production error 구분을 직접 구현.
- "조용한 실패를 시끄럽게 만든다" — s_ib_len 버그(반나절) 같은 유형이 guard 한 줄로 즉시 검출.
- DEM 과의 구분: "개발자 실수는 DET(빌드에서 제거 가능), 환경/HW 이상은 DEM(양산 유지)" 한 문장.
