# LPSPI 지도 (i.MX RT1020 RM Chapter 43)

> P-1a 산출물. 코드 짜기 전 "어떤 순서로 무엇을 건드려야 1 byte가 핀으로 나가는지"의 흐름 정리.
> 출처: NXP i.MX RT1020 Reference Manual, Rev. 2 (01/2021), Chapter 43 Low Power Serial Peripheral Interface (LPSPI).

## 챕터 구조 (어디를 펼칠지)

| 절 | 내용 | 페이지 | 언제 볼지 |
|---|---|---|---|
| 43.2 Overview / Block diagram | 블록도, 기능 | 2301 | 전체 그림 |
| 43.3.1 Master mode | 마스터 동작 흐름 | 2303 | P-1d (init 시퀀스) |
| 43.3.6 Clocks | 기능 클럭 / SCK | — | P-1b |
| 43.3.7 Resets | soft reset / FIFO reset | — | P-1d |
| 43.4 Signal descriptions | SCK/PCS/SOUT/SIN 핀 | 2314 | P-1c (pin mux) |
| 43.5 Memory map and registers | 레지스터 전부 | 2315 | P-1d~e (계속 참조) |

## Base address (RT1020은 LPSPI 4채널)

```
LPSPI1: 0x4039_4000   ← 보통 EVK Arduino 헤더에 묶임. 채널은 P-1c 핀 정하며 확정
LPSPI2: 0x4039_8000
LPSPI3: 0x4039_C000
LPSPI4: 0x403A_0000
```

## 레지스터 메모리 맵 (43.5.1.1) + 우리 입장에서의 역할

| Off | 이름 | 접근 | Reset | 역할 | 등장 |
|---|---|---|---|---|---|
| `0x00` | VERID | RO | `0102_0004h` | 버전. 첫 read로 "버스 살아있나" 확인 | P-1d 첫 sanity |
| `0x04` | PARAM | RO | `0004_0404h` | TX/RX FIFO 깊이 | P-2 |
| `0x10` | **CR** | RW | `0` | 모듈 enable(MEN)/soft reset(RST)/FIFO reset(RTF·RRF) | P-1d |
| `0x14` | **SR** | W1C | `0000_0001h` | 상태 플래그(TDF·RDF·TCF…). polling 핵심 | P-1e |
| `0x18` | IER | RW | `0` | 인터럽트 enable | P-3 |
| `0x1C` | DER | RW | `0` | DMA 요청 enable | P-4 |
| `0x20` | CFGR0 | RW | `0` | 부가 설정(보통 0) | P-1d |
| `0x24` | **CFGR1** | RW | `0` | MASTER 비트, 핀 구성, sample point | P-1d |
| `0x30` | DMR0 | RW | `0` | Data Match 0 (미사용) | — |
| `0x34` | DMR1 | RW | `0` | Data Match 1 (미사용) | — |
| `0x40` | **CCR** | RW | `0` | SCKDIV 등 클럭/delay → SPI 속도 결정 | P-1d |
| `0x58` | **FCR** | RW | `0` | TX/RX FIFO watermark | P-1d |
| `0x5C` | FSR | RO | `0` | FIFO 잔량 | P-1e |
| `0x60` | **TCR** | RW | `0000_001Fh` | FRAMESZ, PRESCALE, CPOL/CPHA, LSBF, PCS선택 = 전송 1건 명령 | P-1d~e |
| `0x64` | TDR | WO | `0` | TX FIFO에 push (write = 송신) | P-1e |
| `0x70` | RSR | RO | `0000_0002h` | RX 상태 | P-1e |
| `0x74` | RDR | RO | `0` | RX FIFO에서 pop (read = 수신) | P-1e |

> 주의 (43.5): RO 레지스터에 write 하거나 WO 레지스터를 read 하면 bus error 가능.
> LPSPI는 값 검증을 안 하므로 SW가 유효한 값만 써야 함.

## RM이 알려준 "왜 이 순서인가" (P-1b~e 정당화)

- **43.3.6 Clocks**: LPSPI는 *기능 클럭(functional clock)* 과 *버스 클럭* 이 분리(asynchronous).
  "functional clock을 프리스케일해서 SCK를 만들고, SCK보다 **최소 2배** 빨라야 한다"고 명시.
  → 레지스터를 만지려면 **버스 클럭(CCM gate)부터 켜야** 한다 = **P-1b가 먼저인 이유**.
  - master mode: SCK 내부 생성 / slave mode: SCK 외부 공급.
- **43.3.7 Resets**: soft reset = `CR[RST]`, FIFO reset = `CR[RTF]`/`CR[RRF]` (둘 다 WO).
  → init 첫 동작 = "CR로 리셋 후 깨끗한 상태에서 시작".
- **43.4 Signals**: 마스터 모드에서 `SCK`(출력), `PCS[0]`(칩셀렉트, 출력), `SOUT`(=MOSI), `SIN`(=MISO).
  → 이 4개 핀을 IOMUXC로 LPSPI에 라우팅 = **P-1c**. 핀이 안 붙으면 레지스터 다 맞아도 신호가 패드로 안 나감.
  - SOUT/SIN은 half-duplex에서 DATA[0]/DATA[1]로도 쓰임 (지금은 무시).
- **43.3.1 Master mode**: 전송은 "TCR로 프레임 명령 세팅 → TDR write → SCK 토글되며 시프트 → RDR read" 로 일어남.
  → **P-1e 송수신의 골격**.

## 한 줄 init 시퀀스 (P-1d에서 코드화할 뼈대)

```
CCM clock gate ON (P-1b) → IOMUXC 핀 라우팅 (P-1c)
  → CR[RST]로 리셋 → CFGR1[MASTER]=1 → CCR[SCKDIV] 속도 설정
  → FCR watermark → CR[MEN]=1 (enable)
  → [전송] TCR 세팅 → TDR write → SR로 완료 polling → RDR read
```

## 면접 narrative

"LPSPI 어떻게 올렸냐" → 이 흐름 그대로:
**클럭(CCM) → 핀(IOMUXC) → 리셋(CR) → 마스터/속도(CFGR1·CCR) → FIFO(FCR) → enable(CR[MEN]) → 전송(TCR/TDR/SR/RDR).**

## 단계 매핑

- P-1b: CCM에서 LPSPI clock root + `CCM_CCGR` gate 비트 찾기 ✅ (아래)
- P-1c: IOMUXC로 SCK/PCS0/SOUT/SIN 핀 라우팅 (채널 확정) — 진행 중 (아래)
- P-1d: CR/CFGR1/CCR/FCR/TCR init 시퀀스
- P-1e: polled 1 byte 송수신 + loopback 자가검증

---

# P-1b: CCM LPSPI 클럭 (RM Chapter 14)

## 전제 (중요)
우리 peripheral 이미지는 startup `IS_BOOTLOADER` 분기라 **`Clock_Init_132MHz()`를 호출 안 함**
→ 보드는 **ROM 기본 클럭** 상태. clock.c 의 CBCMR 코드는 우리 이미지에선 안 돈다.
따라서 LPSPI 클럭은 우리가 **직접** 켜야 한다.

## 두 가지를 켠다: ① 루트(어떤 PLL에서, 얼마로 나눠) ② 게이트(채널 ipg clock ON)

### ① 클럭 루트 — CCM_CBCMR (`0x400F_C018`, reset `2DAA_8324h`)
- `LPSPI_CLK_SEL` (bits 5–4): lpspi clock mux
  - `00` PLL3 PFD1 / `01` PLL3 PFD0 / `10` **PLL2 (=528MHz)** / `11` PLL2 PFD2
- `LPSPI_PODF` (bits 28–26): 분주. `000`=÷1 … `111`=÷8
  - ⚠️ RM NOTE: **PODF는 클럭이 gated 된 상태에서만 바꿀 것** → (게이트 OFF → PODF 설정 → 게이트 ON) 순서
- reset 디코드: SEL=`10`(PLL2), PODF=`011`(÷4) → **LPSPI functional clock = 528/4 = 132 MHz**
- 즉 reset 기본값이 이미 PLL2÷4=132MHz라, 기본값 쓰면 루트는 손 안 대도 됨 (게이트만 켜면 됨).

### ② 클럭 게이트 — CCM_CCGR1 (`0x400F_C06C`, reset `FFFF_FFFF`)
- 채널별 CG 필드 (각 2비트):
  - LPSPI1 → `CG0` (bits 1–0)
  - LPSPI2 → `CG1` (bits 3–2)
  - LPSPI3 → `CG2` (bits 5–4)
  - LPSPI4 → `CG3` (bits 7–6)
- 2비트 인코딩: `00` off / `01` run mode만 ON / `11` **all modes ON (STOP 제외)** → `11` 쓴다.
- reset 가 `FFFF_FFFF` 라 부팅 시 이미 ON 일 가능성 높지만, 명시적으로 `11` 세팅하는 게 정석.

## 자가검증 (P-1b 합격 기준) — ✅ 통과 (2026-06-09)
LPSPI 레지스터 read 자체가 **버스 클럭(ipg_clk_root) gate ON** 을 요구함.
→ 게이트 켠 뒤 **VERID(base+0x0) read** 로 버스 생존 확인.

VERID 필드 (43.5.1.2.4): `MAJOR[31:24] | MINOR[23:16] | FEATURE[15:0]`
- RM 문서 reset value: `0x0102_0004` (MAJOR=01, MINOR=02, FEATURE=0004)
- **실제 칩 read: `0x0101_0004`** (MAJOR=01, MINOR=**01**, FEATURE=0004)
- MAJOR/FEATURE 는 스펙과 정확히 일치, MINOR 만 2→1.

⚠️ 교훈: ID 레지스터의 "reset value" 는 silicon stepping 마다 **MINOR(변동 필드)** 가 다를 수 있다.
sanity 체크는 *정확 일치* 가 아니라 **MAJOR/FEATURE 같은 안정 필드** 로 판단할 것.
0x00000000 / 0xFFFFFFFF / fault 가 아니고 구조적으로 맞는 값이면 → 게이트/버스 정상.

## 채널 메모
LPSPI1 base `0x4039_4000` 가정 (CG0). 채널은 P-1c 핀 가용성으로 최종 확정 —
바뀌면 CG 비트 위치 + base address 만 교체.
`CCM_CBCMR` / `CCM_CCGR1` 매크로는 rt1020_regs.h 에 이미 존재. LPSPI base 는 신규 추가.

---

# P-1c: IOMUXC 핀 라우팅 (RM Chapter 11) — 채널 = LPSPI1

## 시나리오 (확정 2026-06-09): TX-only 자가검증 (배선 0)

### 왜 loopback 안 하나 — 보드 제약
- MIMXRT1020-EVK 의 **Arduino 헤더(J19 등)가 unpopulated** (금색 through-hole만, 핀 미납땜) → 점퍼 바로 못 꽂음.
- LPSPI 코어에 **내부 loopback 비트 없음** (RM loopback 언급은 전부 FlexSPI/SD/SION 건).
- `IOMUXC_LPSPI1_SDI_SELECT_INPUT` daisy 는 `GPIO_AD_B0_13`(=1)/`GPIO_SD_B0_05`(=0)만 → SDO pad 못 잡음 → SION 같은-pad loopback 불가.
- `CFGR1[PINCFG]` half-duplex 단일핀(01/10) 모드는 있으나 자기 echo 보장은 RM 미명시(실험적).
- **결론: P-1e 를 TX-only 로.** TCR/TDR write → `SR[TDF]/[TCF]` + `FSR` FIFO drain 으로 송신엔진(클럭/시프트/FIFO/명령) 검증. RX 는 P-1f device 붙일 때.

### 참고: J19 핀 매핑 (나중 납땜/ device 연결용)
- D11 = `GPIO_AD_B0_12` = LPSPI1_SDO(MOSI) / D12 = `GPIO_AD_B0_13` = LPSPI1_SDI(MISO)
- D13 = `GPIO_AD_B0_10` = SCK / D10 = `GPIO_AD_B0_11` = PCS0
- 진짜 loopback 원하면: D11/D12 구멍에 핀 납땜 후 점퍼.

## LPSPI1 핀 매핑 (RM)
| 신호 | PAD | ALT | 방향(master) | daisy 필요? |
|---|---|---|---|---|
| SCK  | GPIO_AD_B0_10 | ALT1 | 출력 | X |
| PCS0 | GPIO_AD_B0_11 | ALT1 | 출력 | X |
| SDO (MOSI) | GPIO_AD_B0_12 | ALT1 | 출력 | X |
| SDI (MISO) | GPIO_AD_B0_13 | ALT1 | **입력** | **O** (daisy=1) |

## IOMUXC 레지스터 주소 (base 0x401F_8000)
| 레지스터 | 주소 | 값 |
|---|---|---|
| SW_MUX_CTL GPIO_AD_B0_10 (SCK)  | `0x401F_80E4` | `0x1` (ALT1) |
| SW_MUX_CTL GPIO_AD_B0_11 (PCS0) | `0x401F_80E8` | `0x1` (ALT1) |
| SW_MUX_CTL GPIO_AD_B0_12 (SDO)  | `0x401F_80EC` | `0x1` (ALT1) |
| SW_MUX_CTL GPIO_AD_B0_13 (SDI)  | `0x401F_80F0` | `0x1` (ALT1) |
| LPSPI1_SDI_SELECT_INPUT (daisy) | `0x401F_83A4` | `0x1` (=B0_13) |
| SW_PAD_CTL GPIO_AD_B0_10~13 | `0x401F_8258/5C/60/64` | (선택, 초기엔 reset 값으로 충분) |

- MUX_CTL 필드: MUX_MODE[2:0] (ALT1=001), SION[4] (점퍼 쓰므로 불필요).
- SCK/PCS0/SDO 는 master 출력이라 daisy 불필요. SDI 만 입력 daisy 설정.

## EVK 물리 핀 (확인 필요)
RT1020-EVK Arduino 헤더(J19 추정)의 Arduino SPI = LPSPI1:
- D13=SCK(B0_10), D11=MOSI/SDO(B0_12), D12=MISO/SDI(B0_13), D10=PCS0(B0_11)
→ **loopback 점퍼 = D11 ↔ D12**. 실크스크린으로 확인 후 확정.

## 검증
P-1c 단독 런타임 출력 없음 (핀만 mux, LPSPI init 은 P-1d). 빌드 통과로 확인.
TX-only 결정에 따라 SDI mux/daisy 는 실제로 안 쓰임 — forward-compatible 위해 둬도 되고,
최소로 가려면 SCK/SDO/PCS0 3핀만 mux (SDI/daisy 생략) 해도 됨.

---

# P-1d: LPSPI Master init 시퀀스 (RM 43.5.1.x)

## 레지스터 비트 필드 (base 0x4039_4000)
- **CR (0x10)**: MEN[0] 모듈enable / RST[1] soft reset(CR제외 전부 리셋, SW가 clear) / RTF[8] TX FIFO reset / RRF[9] RX FIFO reset
- **CFGR1 (0x24)**: MASTER[0] (1=master) / SAMPLE[1] (고속시 delayed SCK 샘플) / PINCFG[25:24] / OUTCFG[26]
- **CCR (0x40)**: SCKDIV[7:0] / DBT[15:8] / PCSSCK[23:16] / SCKPCS[31:24]
  - **SCK period = (SCKDIV+2) × (functional clock / Prescaler)**. 최소 2 cycle.
  - Prescaler 는 CCR 아님 — **TCR[PRESCALE]** 에 있음 (전송 시 P-1e).
  - 예: funcclk=132MHz, prescale=1 → SCKDIV=130 → SCK ≈ 132/(132) = 1 MHz.
- **FCR (0x58)**: TXWATER[3:0] / RXWATER[19:16]
  - TXWATER=0 → TX FIFO 가 빌 때(count≤0) **SR[TDF]** set.
- **FSR (0x5C, RO)**: TXCOUNT[4:0] / RXCOUNT[20:16] — FIFO 잔량 (P-1e drain 확인용)
- **SR (0x14, W1C)**: reset=0x1 (TDF). 주요: TDF[0] TX data flag / RDF[1] RX data flag / TCF[10] transfer complete / MBF[24] module busy

## init 순서 (MEN=0 상태에서 설정 → 마지막에 MEN=1)
```
CR=RST(1<<1) → CR=0           # soft reset 펄스, 모듈 disable 상태
CFGR1=MASTER(1)               # master 모드
CCR=SCKDIV(130)               # SCK ≈1MHz (prescale=1 가정)
FCR=0                         # TXWATER=0
CR=MEN(1)                     # enable
```
- CFGR1/CCR/FCR 은 반드시 **MEN=0 일 때** 설정 (enable 후 변경 금지).

## P-1d 검증 (readback)
init 후 CFGR1/CCR/SR 읽어 UART 출력:
- CFGR1 = 0x00000001 (MASTER)
- CCR   = 0x00000082 (SCKDIV=130)
- SR    = bit0(TDF) set (FIFO 비어있고 TXWATER=0)
→ 쓴 값이 그대로 읽히면 레지스터 접근 + 설정 OK. 실제 송신은 P-1e.

---

# P-1e: TX-only 송신 (RM 43.5.1.15 TCR)

## TCR (0x60) 비트 필드 — "전송 1건 명령"
- CPOL[31] / CPHA[30] : SPI mode (0/0 = mode0, 기본)
- PRESCALE[29:27] : 000=÷1 … 111=÷128 (CCR SCKDIV 와 곱해져 최종 SCK)
- PCS[25:24] : 00=PCS0
- LSBF[23] : 0=MSB first
- BYSW[22] / CONT[21] / CONTC[20]
- **RXMSK[19]** : 1=수신 데이터 마스킹(RX FIFO에 안 담음) → **TX-only(MISO 없음)에 set**
- TXMSK[18] : 1=송신 마스킹(receive-only 용)
- WIDTH[17:16] : 00=1-bit serial (기본)
- **FRAMESZ[11:0]** : 프레임 비트수 = FRAMESZ+1. **8-bit → FRAMESZ=7**

→ TX-only 8bit mode0: TCR = (1<<19)|(7<<0) = **0x00080007**

## 송신 시퀀스 (polled)
```
SR[TCF] clear (W1C, stale 제거)
TDF(SR bit0) wait        # TX FIFO 자리
TCR = 0x00080007         # 프레임 명령 (TX FIFO에 command enqueue)
TDR = byte               # 데이터 push → 전송 시작
TCF(SR bit10) wait       # 전송 완료 (bounded timeout 권장)
```
- TCR/TDR 둘 다 transmit FIFO 로 감. TCR=명령, TDR=데이터.
- **TCF 가 set 된다 = SCK 가 실제로 8클럭 쳐서 프레임이 나갔다** = TX 엔진(클럭/시프트/FIFO) 정상.
  설정 틀리면 TCF 영원히 안 뜸 → **bounded wait + timeout 메시지**로 hang 방지.

## TX-only 검증 (합격 기준)
- `SR` 에 TCF(bit10) set, MBF(bit24) 다시 0(idle)
- `FSR` TX count(=bits[4:0]) 가 전송 후 0 으로 drain
- timeout 없이 통과 → 송신 경로 OK. (파형 확인은 SCK/SOUT 가 unpopulated 라 생략, 플래그로 대체)
- RX 는 P-1f device 연결 시 검증.

---

# P-2: TX FIFO burst (watermark) — RM 43.5.1.3/13/14

## 개념: 왜 burst 가 빠른가
- **P-1e (per-byte)**: TDR 1개 push → TCF(완료) 대기 → 반복. 매 byte 마다 SCK 가 멈추고 CPU 가 polling.
- **P-2 (burst)**: TX FIFO(16 word)에 미리 여러 byte 를 쌓아둠 → shift register 가 쉬지 않고 연속 출력.
  CPU 는 FIFO 에 자리 날 때만 채우면 됨 → SCK 가 연속으로 흐름.

## PARAM (0x04, RO) — FIFO 깊이
- TXFIFO[7:0] / RXFIFO[15:8] : 깊이 = 2^값. reset 0x040404 → **각 2⁴=16 word**. PCS수[23:16]=4.
- (코드에서 읽어 확인 권장: `(PARAM & 0xFF)` = TXFIFO 지수)

## FCR (0x58) watermark — "언제 채우라고 알릴지"
- TXWATER[3:0]: **SR[TDF] 는 TX FIFO count ≤ TXWATER 일 때 set.**
  - TXWATER=0 → FIFO 빌 때만 TDF (burst 효과 없음, P-1e 와 동일)
  - TXWATER=N → count ≤ N 으로 떨어지면 TDF → "다시 채워라" (IRQ/DMA refill trigger 의 핵심, P-3 로 직결)
- RXWATER[19:16]: SR[RDF] 는 RX FIFO count > RXWATER 일 때 set.

## FSR (0x5C, RO) — 실시간 점유량 (burst 눈으로 보기)
- TXCOUNT[4:0] / RXCOUNT[20:16]. 채운 직후 읽으면 여러 개 쌓여 있음(>1) = burst 증거. 완료 후 0.

## 송신 burst 시퀀스 (polled, TX-only)
```
TCR = 0x80007 (한 번)            # 8bit, RXMSK
for each byte:
    while ((FSR & 0x1F) >= 16) ; # FIFO 가득이면 대기 (자리 날 때까지)
    TDR = byte                   # push
TCF 대기                          # 마지막까지 완료
```
- 검증: 채운 직후 `FSR&0x1F` 가 >1 (여러 개 큐잉됨) → 완료 후 0. + TCF set.
- per-byte 면 FSR 가 항상 ≤1. burst 면 push 속도 > shift 속도라 FIFO 에 쌓임.
