# 📖 [용어 정리] SDRAM 프로젝트 핵심 용어집

i.MX RT1020 베어메탈 SDRAM 초기화 프로젝트(`src/clock.c`, `src/semc.c`, `src/main.c`)에서
실제로 등장하는 용어들을 분류해 정리합니다. 코드를 읽다 막히는 단어가 나오면 이 문서를 먼저 찾아보세요.

> 표기: 레지스터는 `대문자`, 비트 필드는 *기울임*, 비유는 💡로 표시.

---

## 1. 클럭(Clock) 계열

칩에 전원이 들어오면 모든 디지털 회로는 "심장 박동"인 클럭으로 동작합니다.
이 박동을 만들고(PLL), 나누고(Divider), 분배하고(Mux), 켜고 끄는(Gating) 블록이 **CCM**입니다.

### CCM (Clock Controller Module)
- **개념:** 칩 전체 클럭을 만들고 분배하는 "변전소".
- **역할:** PLL 출력을 받아 CPU·버스·각 주변장치(UART, SEMC 등)에 적절한 주파수로 나눠 보냄.

### PLL (Phase-Locked Loop)
- **개념:** 외부 크리스탈의 낮은 기준 주파수(보통 24MHz)를 **체배(곱셈)**해 고속 클럭을 만드는 회로.
- **이 프로젝트:** Sys PLL = 528MHz. 이걸 4분주해 시스템 클럭 132MHz를 만든다.

### PFD (Phase Fractional Divider)
- **개념:** PLL 출력을 **분수 비율**로 다시 나눠 파생 클럭을 만드는 보조 분주기.
- **역할:** 하나의 PLL에서 여러 종류의 클럭(PFD0, PFD1, PFD2…)을 뽑아낼 수 있게 함.

### Clock Tree (클럭 트리) — *주파수* 설정
- **개념:** "소스 선택(Mux) → 분주(Divider) → 최종 주파수"로 이어지는 클럭 분배 경로.
- **이 프로젝트:** `Clock_Init_132MHz()`가 하는 일. PLL 선택 + 분주비 설정.

### Clock Gating (클럭 게이팅) — 모듈 *ON/OFF*
- **개념:** 모듈별 클럭 공급을 켜고 끄는 스위치. **주파수와 무관**, 안 쓰는 모듈의 전력을 차단하는 용도.
- **이 프로젝트:** `LED_Init`, `UART1_Init`, `SDRAM_Clock_Init`이 하는 일.
- ⚠️ **헷갈리지 말 것:** Clock Tree(주파수) ≠ Clock Gating(ON/OFF). 가장 자주 혼동하는 한 쌍.

### CCGR (Clock Gating Register)
- **개념:** Clock Gating을 담당하는 레지스터. `CCGR0`~`CCGR7`까지 있고, 각 레지스터 안에
  *CG0*~*CG15* 필드가 모듈별 스위치로 들어 있음 (2비트 = 11이면 항상 ON).
- **이 프로젝트:** `CCM_CCGR3`의 *CG2* 필드(bit 5:4)를 11로 → SEMC 클럭 ON.

### CBCMR (CCM Bus Clock **Mux** Register)
- **개념:** 어느 클럭 **소스**를 쓸지 선택하는 레지스터(mux).
- **이 프로젝트:** *PRE_PERIPH_CLK_SEL*(bit 19:18) = 00 → 시스템 클럭 소스를 Sys PLL(528MHz)로.
- 💡 "어느 발전소에서 전기를 받을까."

### CBCDR (CCM Bus Clock **Divider** Register)
- **개념:** 받은 클럭을 몇 분주할지 + 클럭 경로를 선택하는 레지스터.
- **이 프로젝트:**
  - *AHB_PODF*(bit 12:10) = 3 → 4분주 (528MHz ÷ 4 = 132MHz)
  - *PERIPH_CLK_SEL*(bit 25) → 메인 경로 / 임시 우회로 전환
- 💡 "받은 전기를 몇 단계 감압할까."

### CDHIPR (CCM Divider **Handshake In-Process** Register)
- **개념:** CBCDR/CBCMR 변경이 하드웨어적으로 끝났는지 알려주는 상태 레지스터.
- **역할:** 해당 BUSY 비트가 0이 될 때까지 폴링해야 안전. 💡 "공사 완료 램프."
- **이 프로젝트:** *AHB_PODF_BUSY*(bit 1), *PERIPH_CLK_SEL_BUSY*(bit 5).

### 클럭 글리치(Glitch)와 우회로
- **개념:** 클럭이 동작 중인데 소스를 바꾸면 박동이 불규칙해지는 현상 → 시스템 멈춤 위험.
- **해결:** ① 안전한 임시 클럭(`periph_clk2`)으로 갈아탐 → ② 소스/분주 변경 → ③ 메인 경로 복귀.
  💡 자동차 변속 시 클러치를 밟는 것과 같음.

### AHB Clock / periph_clk
- **개념:** CPU와 시스템 버스가 사용하는 메인 클럭. 이 프로젝트에선 132MHz.
- **역할:** SEMC도 기본적으로 이 클럭을 받아 SDRAM을 구동.

### PODF (Post Divider Factor)
- **개념:** 분주값을 담는 필드. 레지스터 값 N이면 실제로는 **(N+1) 분주**.
- **예:** `AHB_PODF = 3` → 4분주.

---

## 2. 핀 / IOMUX 계열

하나의 물리적 핀은 GPIO도, UART도, SEMC도 될 수 있습니다.
"이 핀을 무엇으로 쓸지(MUX)"와 "어떤 전기적 특성으로 쓸지(PAD)"를 설정하는 블록이 **IOMUXC**입니다.

### IOMUXC (IO MUX Controller)
- **개념:** 물리 핀의 기능과 전기 특성을 설정하는 블록.
- **구성:** 핀마다 `SW_MUX_CTL`(기능)과 `SW_PAD_CTL`(전기 특성) 두 레지스터가 짝으로 존재.

### MUX 레지스터 (`SW_MUX_CTL_PAD_*`)
- **개념:** 이 핀을 **어떤 기능**으로 쓸지 선택.
- **이 프로젝트:** `GPIO_EMC_00`~`41` 42개 핀을 `0x00`(ALT0 = SEMC)으로 설정.

### PAD 레지스터 (`SW_PAD_CTL_PAD_*`)
- **개념:** 이 핀의 **전기적 특성**(드라이브 강도, 슬루레이트, 풀업/풀다운, 히스테리시스 등) 설정.
- ⚠️ **이 프로젝트 주의점:** 현재 `SDRAM_Pin_Init`은 MUX만 설정하고 PAD는 Boot ROM 기본값에
  의존. 132MHz 고속 동작에서 신호 무결성 마진 문제가 의심될 때 1순위로 점검할 부분.

### ALT Mode (Alternate Function)
- **개념:** 한 핀이 가질 수 있는 기능 후보(ALT0~ALT7) 중 하나를 고르는 것.
- **이 프로젝트:** EMC 핀들은 ALT0가 SEMC 기능. EMC_28의 ALT0 = `SEMC_DQS`.

### SION (Software Input ON)
- **개념:** MUX 레지스터의 비트 4. 출력으로 설정된 핀을 **동시에 입력으로도 되읽도록** 강제.
- **이 프로젝트:** `emc_mux_base[28] = 0x10` → ALT0(=DQS) + SION.
  DQS 신호를 컨트롤러가 루프백으로 되읽어 리드 타이밍을 잡기 위해 필수.

### DQS (Data Strobe)
- **개념:** 데이터와 함께 전송되는 **타이밍 기준 신호**.
- **역할:** 리드 시 컨트롤러가 데이터 라인을 "언제 샘플링할지" 알려주는 기준점.
- **연관:** `SEMC_MCR = 0x10000004`의 DQS pad 루프백 설정과 EMC_28 SION이 한 세트.

---

## 3. SDRAM / SEMC 계열

**SDRAM**은 외부 대용량 휘발성 메모리, **SEMC**는 RT1020이 그 SDRAM을 제어하는 컨트롤러입니다.

### SEMC (Smart External Memory Controller)
- **개념:** RT1020 내장 외부 메모리 컨트롤러. SDRAM, NOR/NAND Flash 등을 제어.
- **이 프로젝트:** SDRAM을 `0x80000000` 주소에 매핑해 일반 메모리처럼 접근 가능하게 함.

### SDRAM (Synchronous DRAM)
- **개념:** 클럭에 동기되어 동작하는 대용량 휘발성 메모리. 셀이 커패시터라 전하가 샘.
- **특징:** 주기적 **리프레시**가 없으면 데이터가 소실됨.

### Row / Column / Bank
- **개념:** SDRAM 내부 주소 구조. 메모리 셀이 행(Row)·열(Column)의 2차원 격자로 배열되고,
  그런 격자가 여러 개의 뱅크(Bank)로 나뉨.
- **접근 순서:** 행 활성화(Activate) → 열 선택(Read/Write) → 행 닫기(Precharge).

### Precharge
- **개념:** 현재 열어둔 행(Row)을 닫는 동작.
- **역할:** 다른 행에 접근하기 전 반드시 선행. 초기화 시 "PRECHARGE ALL"로 전체 행을 닫음.

### Auto Refresh
- **개념:** SDRAM 셀(커패시터)의 전하를 주기적으로 다시 채워주는 동작.
- **역할:** 안 하면 시간이 지나며 데이터가 소실됨. 초기화 시퀀스에서 2회 수행 후, 이후 자동 수행.
- ⚠️ **테스트 함정:** 쓰고 즉시 읽으면 리프레시가 틀려도 통과함. 보존(retention) 검증은
  쓰기 후 일정 시간 대기 뒤 다시 읽어야 함.

### MRS (Mode Register Set)
- **개념:** SDRAM에 동작 모드(CAS Latency, Burst Length 등)를 알려주는 초기화 명령.
- **이 프로젝트:** 모드 값 `0x33`(BL=8, sequential, CL=3)을 `SEMC_IPTXDAT`로 전달.

### CL (CAS Latency)
- **개념:** 읽기 명령을 내린 뒤 실제 데이터가 나오기까지 걸리는 클럭 수.
- **이 프로젝트:** CL=3. `SDRAMCR0`와 MRS 값 양쪽에 일관되게 설정되어 있어야 함.

### BL (Burst Length)
- **개념:** 한 번의 명령으로 연속 전송되는 워드의 개수.
- **이 프로젝트:** BL=8.

### tRP / tRAS / tRC / tRFC (타이밍 파라미터)
- **개념:** SDRAM 데이터시트에 ns 단위로 명시된 동작 타이밍 제약.
  - **tRP:** Precharge 후 다음 명령까지 대기
  - **tRAS:** 행 활성화 유지 최소 시간
  - **tRC:** 행 사이클 전체 시간
  - **tRFC:** Refresh 명령 완료까지 시간
- **변환:** `ns 값 ÷ SEMC 클럭 주기`로 클럭 수를 구해 `SDRAMCR1/2/3`에 채움.
- ⚠️ **이 프로젝트 주의점:** 현재 `SDRAMCR1/2/3`은 출처 없는 매직 넘버. 실장된 SDRAM 칩
  품번을 확인해 데이터시트 기준으로 재계산하고 계산 근거를 주석으로 남기는 것이 권장됨.

### BR (Base Register)
- **개념:** 이 메모리를 CPU 주소 공간의 어디에, 얼마 크기로 매핑할지 정하는 레지스터.
- **이 프로젝트:** `SEMC_BR0` → 시작 `0x80000000`, 크기 32MB, valid 비트 ON.

### IPCMD (IP Command)
- **개념:** SEMC가 SDRAM에 직접 명령(Precharge/Refresh/MRS 등)을 보내는 통로.
- **형식:** `0xA55A0000 | 명령코드`. 앞의 `0xA55A`는 오작동 방지용 키.
- **명령코드 예:** `0x0F` PRECHARGE ALL, `0x0C` AUTO REFRESH, `0x0A` MRS.

### IPTXDAT
- **개념:** IP 명령에 데이터를 함께 실어 보낼 때 쓰는 레지스터.
- **이 프로젝트:** MRS 명령 시 모드 레지스터 값(`0x33`)을 여기에 담아 전달.

### INTR (Interrupt/Status Register)
- **개념:** IP 명령의 완료/에러 상태를 알려주는 레지스터.
- **이 프로젝트:** *IPCMDDONE*(bit 0) 완료, *IPCMDERR*(bit 1) 에러. 명령 후 이 비트를 폴링.

---

## 4. 자주 혼동되는 개념 한눈에

| 헷갈리는 쌍 | A | B |
|---|---|---|
| 클럭 | **Clock Tree** = 주파수 설정 | **Clock Gating** = 모듈 ON/OFF |
| 핀 설정 | **MUX** = 어떤 기능으로 | **PAD** = 어떤 전기 특성으로 |
| SDRAM 초기화 | **Precharge** = 행 닫기 | **Refresh** = 전하 재충전 |
| CCM 레지스터 | **CBCMR** = 소스 선택 | **CBCDR** = 분주/경로 |

---

## 5. 이 프로젝트 초기화 흐름과 용어 매핑

```
[전원 ON]
   └─ Boot ROM → IVT/FCB → SystemInit() → main()
[main() 진입]
   1. delay_loop()          : 디버거 접속 대기 윈도우
   2. Clock_Init_132MHz()   : Clock Tree — CBCMR(소스)+CBCDR(분주) → 132MHz
   3. LED_Init / UART1_Init : Clock Gating(CCGR) + IOMUX
   4. SDRAM_Clock_Init()    : Clock Gating — CCGR3 CG2 → SEMC 클럭 ON
   5. SDRAM_Pin_Init()      : IOMUX MUX 42핀 ALT0(SEMC), EMC_28 = DQS+SION
   6. SDRAM_Init_Sequence() : SEMC_MCR/BR0/SDRAMCR0~3 설정
                              → Precharge → Refresh×2 → MRS → Refresh enable
   7. SDRAM_Test16/32()     : 0x80000000 영역 Read/Write 무결성 검증
```

---

> 📌 관련 문서: `MCU_BASIC.md`(부팅 아키텍처), `MEMORY_MAP.md`(주소 맵),
> `GPIO_REGISTERS.md`(GPIO 레지스터). 새 용어가 나오면 이 문서에 계속 추가하세요.
