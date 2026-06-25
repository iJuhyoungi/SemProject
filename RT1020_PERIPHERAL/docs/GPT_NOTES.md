# GPT 정리 (i.MX RT1020 RM Chapter 47)

> P-10 산출물. 다섯 번째 peripheral. 타이머라 출력 파형이 아니라 **시간/주기 그 자체**를 다룬다.
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 47 General Purpose Timer (GPT).

## PART 1 — GPT 가 무엇이고 왜 다른가
- 지금까지(SPI/ADC/PWM/CAN)는 데이터를 주고받는 통신·변환기. **GPT 는 시간을 세는 기계.**
- 핵심은 **32-bit free-running up-counter(GPT_CNT)** 하나 + **12-bit 프리스케일러(PR)**. 클럭이 들어오면 0 부터 계속 증가.
- **Output Compare**: 카운터가 OCR(비교값)에 도달하면 이벤트 발생(플래그 set + 인터럽트 + 핀 동작 선택). → 이걸로 **정확한 주기 인터럽트** 생성.
- **Input Capture**: 입력 핀에 엣지가 들어온 순간의 카운터 값을 ICR 에 박제. → 외부 신호의 주기/펄스폭 측정(이건 Icu/QuadTimer 쪽 개념, P-12 에서).
- AUTOSAR **Gpt** 드라이버 = 이 GPT. OS 스케줄러 tick, 주기 task 의 시간 기준. 임베디드 RTOS 의 심장박동.

## PART 2 — Restart vs Free-Run (주기 인터럽트의 핵심)

GPT 는 0 부터 증가하는 32-bit up-counter(CNT) 하나뿐이고, 비교값(OCR1~3)에 도달하면 "compare 이벤트"(플래그 set + 인터럽트)가 난다. 갈림길은 **이벤트 후 카운터가 어떻게 행동하느냐** — 이걸 CR[FRR] 한 비트가 가른다. 주기 타이머냐 자유 시간축이냐가 여기서 갈린다.

### Restart mode (FRR=0) — 주기 타이머
- CNT 가 **OCR1** 에 도달하면 compare 이벤트가 나고 **CNT 가 0 으로 리셋되어 다시 센다** (RM §47.5.1.1).
- 결과적으로 0 ↔ OCR1 사이를 톱니파처럼 반복 → **한 주기 = (OCR1 + 1) tick**, 소프트웨어가 손대지 않아도 자동 반복.
- 그래서 OF1 인터럽트가 **일정 간격으로 계속** 들어온다. 주기 인터럽트(OS tick)는 이 모드여야 한다.
- 타임라인 (OCR1 = 1000):

  ```
  CNT: 0,1,…,1000 ─[OF1 + 리셋]→ 0,1,…,1000 ─[OF1 + 리셋]→ …
       └──── 1001 tick ────┘    └──── 1001 tick ────┘   (간격 고정)
  ```

### Free-Run mode (FRR=1) — 자유 시간축
- compare 이벤트가 나도 **카운터는 리셋되지 않는다.** 0xFFFF_FFFF 까지 끝까지 세고 롤오버(ROV 플래그)하여 0 으로 돌아온다 (RM §47.5.1.2).
- OCR1~3 는 **하나의 긴 32-bit 시간축 위에 찍은 독립 마커 3개**일 뿐.
- 그래서 OF1 은 부팅 후 OCR1 을 통과하는 **딱 한 번** 나고, 다음 OF1 은 카운터가 2³² 를 한 바퀴 다 돈 뒤에야 다시 OCR1 에 도달할 때 발생 → 사실상 주기가 2³² tick(고정·거대). **주기 인터럽트용으로는 못 쓴다.**
- 굳이 주기를 만들려면 **ISR 에서 매번 `OCR1 += 주기`** 로 다음 데드라인을 직접 옮겨야 한다. 대신 마커 3개로 **서로 다른 시각의 이벤트 3개**를 타이머 하나로 관리 가능 → SW 타이머 다중화·timestamp·경과시간 측정에 적합.

### OCR1 만 특별한 이유 / 함정
- **리셋 기능은 Compare Channel 1(OCR1) 전용.** OCR2/OCR3 는 compare 이벤트만 내고 카운터를 리셋하지 않는다 (RM §47.5.1.1). → restart 모드에서 주기를 정하는 건 언제나 OCR1.
- **OCR1 에 write 하면 그 순간 카운터가 즉시 리셋된다** (RM). 주기를 높은 값 → 낮은 값으로 바꿀 때 compare 이벤트를 놓치지 않으려는 하드웨어 보호. 동작 중 주기를 바꾸면 그 시점에 한 주기가 잘린다는 뜻이니 알아둘 것.

### P-10 의 선택
- **Restart(FRR=0) + OCR1 = 주기.** OF1 인터럽트가 일정 간격으로 들어오고, ISR 은 OF1 플래그만 W1C 로 지우면 다음 주기가 자동으로 굴러간다. (ENMOD 와는 별개 — ENMOD 는 "EN=1 순간 0 부터 시작할지"만 정함, FRR 은 "compare 후 리셋할지".)

## PART 3 — 클럭 소스 (CR[CLKSRC], bits 8:6)

카운터에 들어갈 클럭을 CLKSRC[8:6] 3비트로 고른다.

| 값 | 소스 | 비고 |
|---|---|---|
| 000 | No clock | 정지 |
| 001 | Peripheral Clock (ipg_clk) | 게이트만 켜면 바로 사용 → **P-10 선택** |
| 010 | High Freq Reference | ipg_clk_highfreq |
| 011 | External Clock | 외부 핀 입력 |
| 100 | Low Freq (32 kHz) | 저전력용 |
| 101 | Oscillator 24 MHz | EN_24M[10] + PRESCALER24M 추가 설정 필요 |

- **CLKSRC 변경은 EN=0 일 때만.** (RM 주의 — 동작 중 소스 변경 금지)
- P-10 은 **001 (ipg_clk)** 선택. CCGR1 게이트만 켜면 되어 가장 단순. 정확한 주파수는 ROM 기본 클럭에 의존하지만, "카운터 진행 + 주기 IRQ 카운트" 검증에는 영향 없음.

## base / 클럭 게이트 / IRQ
- **GPT1 base: `0x401E_C000`** (GPT2: 0x401F_0000)
- 클럭 게이트: **CCM_CCGR1** — CG10(bits 21:20)=gpt1 bus clock(gpt_clk_enable) + CG11(bits 23:22)=gpt1 serial clock. **둘 다 11 로** (bus=레지스터접근, serial=카운팅클럭).
- **IRQ 번호: GPT1 = 100** → 벡터 인덱스 16+100 = **116**. NVIC: 100/32=3 → **ISER3 의 bit (100%32)=4**.

## 레지스터 맵 (GPT1, base 0x401E_C000)

| off | 이름 | 접근 | 역할 / 핵심 비트 |
|---|---|---|---|
| 0x00 | **CR** | RW | EN[0] / ENMOD[1] / **CLKSRC[8:6]** / **FRR[9]** / SWR[15](소프트리셋) / OMn(출력핀동작) / IMn(캡처엣지) |
| 0x04 | **PR** | RW | PRESCALER[11:0] (÷(PRESCALER+1)), PRESCALER24M[15:12] |
| 0x08 | **SR** | W1C | **OF1[0]** / OF2[1] / OF3[2] / IF1[3] / IF2[4] / ROV[5] — 이벤트 플래그(1 써서 클리어) |
| 0x0C | **IR** | RW | OF1IE[0] / … / ROVIE[5] — 인터럽트 enable (SR 과 같은 비트 위치) |
| 0x10~0x18 | **OCR1~3** | RW | Output Compare 값 (리셋 0xFFFF_FFFF). **OCR1 = 주기** |
| 0x1C~0x20 | ICR1~2 | RO | Input Capture 값 (P-12 에서 사용) |
| 0x24 | **CNT** | RO | 현재 카운터 값 (동작 확인용) |

- 주의: RO 레지스터(ICR1/2, CNT)에 **쓰면 bus exception**. SR 은 W1C.

## init 시퀀스 (주기 인터럽트)
```
1) 클럭     CCGR1 |= (3<<20)|(3<<22)        # CG10(bus)+CG11(serial) on
2) 소프트리셋 CR[SWR]=1 → 자동 클리어 대기   # 깨끗한 시작(EN/ENMOD 등 일부 제외)
3) 정지+모드 CR: EN=0 상태에서 CLKSRC=001(ipg), FRR=0(restart), ENMOD=1(EN 시 0부터)
4) 프리스케일 PR = 분주값                     # 카운팅 클럭 ÷(PR+1)
5) 주기      OCR1 = 주기값                     # CNT 가 여기 도달 시 OF1 + 리셋
6) 인터럽트  IR |= (1<<0)                       # OF1IE: OC1 인터럽트 enable
            NVIC ISER3 |= (1<<4)               # IRQ100 enable
            벡터테이블 idx116 = GPT1_IRQHandler (startup 정적 추가)
7) 시작      CR[EN]=1                           # 카운트 시작
8) ISR       SR=(1<<0)                          # OF1 W1C 클리어 + 카운트++
```

## 검증 (scope/LA 없이)
- **무도구 1차**: `CNT` 를 두 번 읽어 값이 증가하면 카운터·클럭 동작 확인 (PWM 의 SM0CNT, SPI TX-only 와 같은 내부상태 검증).
- **무도구 2차(진짜 시연)**: OF1 인터럽트가 주기적으로 들어와 **ISR 카운터가 증가** → main 에서 그 값을 UART 로 출력. "타이머가 자율적으로 시간을 세고 CPU 를 주기적으로 깨운다" 실증.
- IRQ 3관문은 P-3(SPI IRQ) 와 동일 구조: IR(소스) / NVIC ISER(라인) / 벡터테이블(핸들러 주소). GPT 는 IRQ100 이라 ISER3·idx116 로 위치만 다름.

## 계획
- P-10a: 개념+지도 ✅ (지금) → 본 문서
- P-10b: 클럭→리셋→CLKSRC/FRR→PR/OCR1→IR/NVIC→EN. CNT 진행 + 주기 IRQ 카운트로 검증
- (P-10c 선택) AUTOSAR Gpt API (Gpt_Init / Gpt_StartTimer / Gpt_EnableNotification) — Phase 2 에서

## AUTOSAR 연결
- AUTOSAR **Gpt** 드라이버 = 이 GPT. OsTick(스케줄러 시간기준)·주기 알람의 토대. `Gpt_StartTimer`/`Gpt_EnableNotification` 이 핵심 API. 본업 RTOS·스케줄링 narrative 와 직결.
