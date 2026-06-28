# QuadTimer (TMR) 정리 — Icu (i.MX RT1020 RM Chapter 49)

> P-12 산출물. 일곱 번째 peripheral, Phase 1 의 마지막이자 가장 까다로운 경계.
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 49 Quad Timer (TMR).

## PART 1 — Icu / 입력 캡처가 무엇인가
- AUTOSAR **Icu**(Input Capture Unit) = **외부 신호를 측정**하는 드라이버. GPT(시간 생성)의 거울상 — GPT 는 시간을 *만들고*, Icu 는 들어온 신호의 시간을 *읽는다*.
- 주요 모드: ① 엣지 검출 ② **신호 측정**(주기/duty — 엣지에서 카운터 값을 래치해 차이 계산) ③ **엣지 카운팅** ④ 타임스탬프.
- RT1020 대응 = **QuadTimer(TMR)**. 채널 4개(TIMER0~3) 각각 카운터 + 캡처 레지스터 보유.

## PART 2 — 핵심 난관: 입력 신호가 필요하다
- 진짜 "입력 캡처"는 **신호가 채널 입력에 들어와야** 엣지에서 CNTR 를 CAPT 로 래치한다. 그런데 EVK 헤더가 unpopulated → 외부 신호원·점퍼 불가.
- **해결책 두 갈래:**
  1. **(채택) Cascade — 부품/XBAR 없이 자가완결.** CTRL[PCS] 에 "Counter N output"(0100~0111) 옵션이 있다. → **ch0 가 사각파를 만들고, ch1 이 그 출력을 내부에서 직접 카운트.** 칩 안에서 신호 생성+측정이 닫힌다. = Icu 의 **엣지 카운팅** 모드 실증.
  2. **(경계/선택) 진짜 래치-캡처.** SCTRL[CAPTURE_MODE] 로 엣지마다 CNTR→CAPT 래치 → 연속 CAPT 차이 = 주기. 단 캡처는 **채널 입력 핀**에서만 트리거되므로, 신호를 입력 핀에 넣으려면 **XBARA 내부 라우팅**(PWM/TMR출력→XBAR→TMR입력)이 필요. Phase 1 을 여기서 마무리하거나, 여유되면 도전.

## PART 3 — Cascade 가 동작하는 원리 (CTRL[PCS])
QuadTimer 각 채널의 카운터는 **primary count source(PCS)** 가 셀 대상을 정한다. 목록에 "다른 채널의 출력"이 있다:

| PCS | count source |
|---|---|
| 0000~0011 | Counter 0~3 **입력 핀** |
| 0100~0111 | Counter 0~3 **출력** ← 내부 연결! |
| 1000~1111 | IP bus clock ÷1 / ÷2 / … / ÷128 |

- ch0: PCS=IP bus clock(예 ÷128) + OUTMODE=011(compare 시 OFLAG 토글) → **OFLAG 가 사각파**.
- ch1: PCS=0100(Counter 0 출력) + CM=001(primary rising 카운트) → **ch0 사각파 엣지를 셈.** CNTR1 이 증가 = 내부 신호를 측정.

## base / 클럭 / 레지스터
- **TMR1 base: `0x401D_C000`** (TMR2 별도)
- 클럭 게이트: **CCM_CCGR6 CG13 (bits 27:26)** = qtimer1_clk_enable. (CCGR6 = 0x400F_C080, rt1020_regs.h 에 신규 추가)
- 채널 N 레지스터 = base + **N×0x20**:

| off | 이름 | 역할 |
|---|---|---|
| 0x00 | COMP1 | 비교값 1 (사각파 주기 결정) |
| 0x02 | COMP2 | 비교값 2 |
| 0x04 | **CAPT** | 캡처값 (입력 엣지에 CNTR 래치) |
| 0x06 | LOAD | 카운터 재초기화 값 |
| 0x0A | **CNTR** | 현재 카운터 (RO 관측) |
| 0x0C | **CTRL** | CM[15:13]/PCS[12:9]/SCS[8:7]/LENGTH[5]/DIR[4]/**OUTMODE[2:0]** |
| 0x0E | **SCTRL** | TCF/TCFIE/**CAPTURE_MODE**/IEF/IPS/OEN(출력 enable) |

- CTRL[CM]: 001=primary rising / 010=both edges / … . CTRL[OUTMODE]: 011=toggle OFLAG on compare(사각파).
- SCTRL[CAPTURE_MODE]: 00=off / 01=rising / 10=falling / 11=both. SCTRL[OEN]=출력 핀 enable.

## init / 데모 시퀀스 (Cascade — 자가완결)
```
1) 클럭     CCGR6 |= (3<<26)                 # CG13 qtimer1
2) ch0(신호 생성)
   CTRL0  = 0                                 # 먼저 정지
   LOAD0  = 0
   COMP10 = 사각파 반주기 값
   CTRL0  = CM(001) | PCS(1000+, bus÷N) | OUTMODE(011 toggle) | LENGTH(1)
            → OFLAG0 가 일정 주기로 토글
3) ch1(측정)
   CTRL1  = 0
   CTRL1  = CM(001 rising) | PCS(0100, Counter0 출력)
            → CNTR1 이 ch0 엣지를 카운트
4) 관측: CNTR1 을 시간차 두고 두 번 읽어 증가 확인
   (선택) ch1 SCTRL[CAPTURE_MODE] 로 CAPT 래치까지 — 단 입력 트리거 필요(XBAR)
```

## 검증 (도구 없이)
- **무도구**: `CNTR1` 을 beat 간격으로 읽어 **증가하면** ch0 사각파를 ch1 이 세고 있다는 것 = 내부 신호 측정 동작. (PWM 의 SM0CNT, GPT 의 tick 과 같은 내부상태 검증 계열.)
- ch0 OFLAG 사각파 자체는 핀(scope) 없이는 못 보지만, ch1 의 카운트 증가가 그 존재를 간접 증명.
- (래치-캡처 도전 시) CAPT 두 값의 차이 = 신호 주기 → 진짜 Icu 신호 측정.

## 계획
- P-12a: 개념+지도 ✅ (지금) → 본 문서
- P-12b: CCGR6 게이트 → ch0 사각파 생성 → ch1 cascade 카운트 → CNTR1 증가로 검증. (Phase 1 마무리)
- (선택) XBARA 라우팅으로 진짜 래치-캡처(주기 측정)까지 — 경계 도전
- (P-12c 선택) AUTOSAR Icu API — Phase 2 에서

## AUTOSAR 연결
- AUTOSAR **Icu** 드라이버 = 이 QuadTimer. 엔진 회전수(크랭크 센서)·PWM 입력 측정·주파수 측정 등 차량 입력 신호 계측의 기본. `Icu_StartSignalMeasurement`/`Icu_GetTimeElapsed`/`Icu_GetEdgeNumbers` 가 핵심 API — 본 데모의 cascade 카운트는 `Icu_GetEdgeNumbers`(엣지 카운팅) 에 해당.
