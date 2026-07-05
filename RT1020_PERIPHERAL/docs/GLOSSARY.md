# 용어집 (RT1020_PERIPHERAL 약어 정리)

> 이 프로젝트(베어메탈 MCAL 드라이버)에서 쓴 레지스터·비트·개념 약어 모음.
> "약어 / 풀이 / 설명" 순. RM = i.MX RT1020 Reference Manual.

## 1. 공통 개념 · 표기

| 약어 | 풀이 | 설명 |
|---|---|---|
| **base address** | — | 페리페럴 레지스터 블록의 시작 주소. 각 레지스터 = base + offset |
| **offset** | — | base 로부터의 상대 위치 (예: CR = base + 0x00) |
| **reset value** | — | 전원/리셋 직후 레지스터의 기본값 |
| **RO / RW / WO** | Read-Only / Read-Write / Write-Only | 레지스터·비트의 접근 권한 |
| **W1C** | **W**rite **1** to **C**lear | 1 을 써야 비트가 지워짐 (0 쓰면 무시). 플래그 클리어 표준 |
| **write-once** | — | 리셋 후 단 한 번만 쓸 수 있는 비트 (안전 설계). 변경하려면 unlock |
| **bit [n]** | — | n 번 비트 하나. `(1u<<n)` |
| **[hi:lo]** | — | hi~lo 비트로 이뤄진 필드 (예: `[9:8]` = 2비트 값 0~3) |
| **atomic write** | — | 한 store 로 끝나 중간에 끼어들 수 없는 쓰기 |
| **polling** | — | CPU 가 플래그를 계속 읽으며 기다림 (가장 단순, CPU 묶임) |
| **IRQ** | **I**nterrupt **R**e**Q**uest | 하드웨어가 CPU 를 깨우는 인터럽트 요청 |
| **ISR** | **I**nterrupt **S**ervice **R**outine | 인터럽트 발생 시 실행되는 핸들러 함수 |
| **NVIC** | **N**ested **V**ectored **I**nterrupt **C**ontroller | Cortex-M7 의 인터럽트 컨트롤러 |
| **ISER** | **I**nterrupt **S**et-**E**nable **R**egister | NVIC 의 IRQ 허용 레지스터. IRQn → ISER(n/32) 의 bit(n%32) |
| **vector table** | — | 인터럽트별 핸들러 주소 표. IRQn → 인덱스 16+n |
| **DWT CYCCNT** | **D**ata **W**atchpoint and **T**race, **CYC**le **C**ou**NT** | 코어 사이클 카운터. poll/IRQ/DMA CPU 비용 측정에 사용 |

## 2. 클럭 · 핀 · 리셋 (시스템)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **CCM** | **C**lock **C**ontroller **M**odule | 칩의 클럭 생성·분배·게이팅 담당 |
| **CCGR** | **C**lock **C**ontrol **G**ating **R**egister | 페리페럴별 클럭 게이트. 켜야 레지스터 접근 가능 |
| **CG** | **C**lock **G**ate | CCGR 안의 개별 게이트 번호. 1개당 2비트 → `11`(=3)로 켬 (예: `CCGR5[CG2]`=bits 5:4) |
| **ipg_clk** (bus clock) | **I**P **G**asket clock | 페리페럴 레지스터 접근 버스 클럭 |
| **PLL** | **P**hase-**L**ocked **L**oop | 기준 클럭을 체배해 고속 클럭 생성 (예: PLL2=528MHz) |
| **LPO** | **L**ow-**P**ower **O**scillator | 저전력 내부 32kHz 발진기. 메인 클럭 죽어도 도는 독립 클럭 |
| **PODF** | **P**ost **D**ivider **F**actor | 클럭 후단 분주 계수 |
| **IOMUXC** | **IO** **MU**ltiple**X** **C**ontroller | 핀 기능 선택(muxing) 모듈 |
| **ALT** | **ALT**ernate function | 핀의 대체 기능 번호 (예: LPSPI=ALT1) |
| **SRC** | **S**ystem **R**eset **C**ontroller | 칩 리셋 관장 모듈 (base 0x400F_8000) |
| **SRSR** | **S**RC **R**eset **S**tatus **R**egister | 직전 리셋 원인을 비트로 기록 (W1C) |
| **POR** | **P**ower-**O**n **R**eset | 전원 인가 리셋 (SRSR bit0) |

## 3. LPSPI / AUTOSAR Spi (Ch.43)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **LPSPI** | **L**ow-**P**ower **SPI** | RT1020 의 SPI 페리페럴 |
| **VERID** | **VER**sion **ID** | 버전 식별 레지스터 (sanity check 용) |
| **PARAM** | **PARAM**eter | FIFO 깊이 등 하드웨어 파라미터 |
| **CR** | **C**ontrol **R**egister | 모듈 제어 (RST/MEN) |
| **CFGR1** | **C**on**F**i**G**uration **R**egister 1 | MASTER 모드 등 설정 |
| **CCR** | **C**lock **C**onfiguration **R**egister | SCK 분주(SCKDIV) |
| **TCR** | **T**ransmit **C**ommand **R**egister | 프레임 크기·모드·RXMSK 등 송신 명령 |
| **FCR / FSR** | **F**IFO **C**ontrol / **S**tatus **R**egister | FIFO watermark 설정 / 현재 채움 상태 |
| **TDR / RDR** | **T**ransmit / **R**eceive **D**ata **R**egister | 송/수신 데이터 |
| **SR** | **S**tatus **R**egister | 상태 플래그 |
| **IER / DER** | **I**nterrupt / **D**MA **E**nable **R**egister | 인터럽트 / DMA 요청 enable |
| **TDF / TCF** | **T**ransmit **D**ata **F**lag / **T**ransfer **C**omplete **F**lag | FIFO 빔 / 전송 완료 |
| **MEN** | **M**odule **EN**able | 모듈 켜기 (설정은 MEN=0 일 때만) |
| **SCKDIV** | **SCK** **DIV**ider | SPI 클럭 분주 |
| **RXMSK / FRAMESZ** | **RX** **M**a**SK** / **FRAME** **S**i**Z**e | 수신 마스킹 / 프레임 비트 수 |
| **watermark** | — | FIFO 가 이 수준까지 비/차면 플래그·요청 발생 |

### eDMA / DMAMUX (SPI DMA 전송)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **eDMA** | **e**nhanced **DMA** | CPU 개입 없이 메모리↔페리페럴 전송 엔진 |
| **DMAMUX** | **DMA** **MU**ltiple**X**er | DMA 요청 소스를 채널에 연결 |
| **TCD** | **T**ransfer **C**ontrol **D**escriptor | eDMA 채널의 "전송 명령서" |
| **SADDR / DADDR** | **S**ource / **D**estination **ADDR**ess | 출발/도착 주소 |
| **SOFF / DOFF** | **S**ource / **D**estination **OFF**set | 전송 1회 후 주소 증가량 |
| **NBYTES** | **N**umber of **BYTES** | minor loop 1회 바이트 수 |
| **CITER / BITER** | **C**urrent / **B**eginning **ITER**ation | major loop 남은/시작 횟수 |
| **CSR** | **C**ontrol and **S**tatus **R**egister | TCD 제어 (DREQ/INTMAJOR) |
| **SERQ / CINT** | **S**et **E**nable **R**e**Q**uest / **C**lear **INT** | 채널 enable / 인터럽트 클리어 |
| **DREQ** | **D**isable **REQ**uest | major loop 끝나면 요청 자동 해제 |

## 4. ADC (Ch.61)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **ADC** | **A**nalog-to-**D**igital **C**onverter | 아날로그 전압 → 디지털 값 |
| **CFG** | **C**on**F**i**G**uration | 분주(ADIV)/클럭(ADICLK)/해상도(MODE) 설정 |
| **GC** | **G**eneral **C**ontrol | 보정(CAL) 등 |
| **HC0** | **H**ardware **C**ontrol 0 | 변환할 입력 채널 선택 + 트리거 |
| **HS** | **H**ardware **S**tatus | 변환 완료(COCO) 상태 |
| **R0** | **R**esult 0 | 변환 결과 값 |
| **COCO** | **CO**nversion **CO**mplete | 변환 완료 플래그 |
| **CAL** | **CAL**ibration | 변환 정확도 보정 |
| **VREFSH** | **V**oltage **REF**erence **S**elect **H**igh | 내부 기준전압(상한) — 핀 없이 self-test 채널 |
| **ADIV / ADICLK / MODE** | **A**DC **DIV**ide / **I**nput **CLK** / — | 클럭 분주 / 입력 클럭 소스 / 해상도(10b=12-bit) |

## 5. eFlexPWM (Ch.50)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **PWM** | **P**ulse **W**idth **M**odulation | 펄스 폭으로 듀티/주파수 제어 |
| **SM** | **S**ub**M**odule | 독립 카운터 단위 (SM0~3) |
| **INIT / VAL1~5** | **INIT**ial / **VAL**ue | 카운터 초기값 / 주기·엣지 비교값 (VAL1=주기, VAL2/3=PWM_A 엣지) |
| **CNT** | **C**ou**NT**er | 현재 카운터 (동작 확인용) |
| **CTRL / PRSC / FULL** | **C**on**TR**o**L** / **PR**e**SC**aler / — | 제어 / 분주 / full-cycle reload(버퍼 로드 위해 필요) |
| **LDOK** | **L**oa**D** **OK** | 더블버퍼된 INIT/VAL 을 실제 반영 |
| **RUN** | — | 서브모듈 클럭 시작 |
| **OUTEN / MCTRL** | **OUT**put **EN**able / **M**aster **C**on**TR**o**L** | 출력 핀 enable / 모듈레벨 제어(LDOK/RUN) |
| **duty** | duty cycle | HIGH 비율 = (VAL3-VAL2)/주기 |

## 6. FlexCAN (Ch.40)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **CAN** | **C**ontroller **A**rea **N**etwork | 멀티노드 차량 버스 프로토콜 |
| **MCR** | **M**odule **C**onfiguration **R**egister | MDIS/SOFTRST/FRZ/HALT/FRZACK/NOTRDY/MAXMB |
| **CTRL1** | **C**on**TR**o**L** 1 | 비트타이밍 + LPB(loopback) |
| **MB** | **M**essage **B**uffer | 메시지 슬롯(메일박스). 각 16바이트, ID/DLC/데이터 보관 |
| **CODE** | — | MB 역할: 0xC=TX, 0x4=RX EMPTY(대기), 0x2=FULL(수신됨) |
| **ID / DLC** | **ID**entifier / **D**ata **L**ength **C**ode | 프레임 식별자(우선순위) / 데이터 길이(0~8B) |
| **IDE / SRR / RTR** | **ID** **E**xtended / **S**ubstitute **R**emote **R**equest / **R**emote **T**ransmission **R**equest | 확장ID 여부 / (표준프레임 예약) / 원격요청 |
| **PRESDIV / PROPSEG / PSEG / RJW** | **PRE**scaler **DIV** / **PROP**agation **SEG** / **P**hase **SEG** / **R**esync **J**ump **W**idth | 비트타이밍 세그먼트 (모든 노드 동기화) |
| **LPB** | **L**oo**PB**ack | TX↔RX 내부 직결 → 부품 없이 송수신 검증 |
| **FRZ / HALT / FRZACK** | **FR**ee**Z**e / — / **FR**ee**Z**e **ACK** | Freeze 모드 진입 요청 / 정지 / 진입 확인 |
| **NOT_RDY / SOFTRST / MAXMB** | **NOT** **R**ea**DY** / **SOFT** **R**e**S**e**T** / **MAX** **MB** | 동기화 전 / 소프트리셋 / 사용 MB 최대 번호 |
| **IFLAG1** | **I**nterrupt **FLAG** 1 | MB 완료 플래그 (W1C) |
| **RXMGMASK** | **RX** **M**ailboxes **G**lobal **MASK** | RX ID 필터 마스크 (reset=전비트 비교) |
| **Freeze Mode** | — | 설정 변경은 반드시 이 모드에서 (버스와 비동기 안전상태) |

## 7. GPT — General Purpose Timer (Ch.47)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **GPT** | **G**eneral **P**urpose **T**imer | 시간 생성 타이머 (OS tick 토대) |
| **CR / PR / SR / IR** | **C**ontrol / **PR**escaler / **S**tatus / **I**nterrupt **R**egister | 제어 / 분주 / 상태 / 인터럽트 enable |
| **OCR1~3 / ICR1~2** | **O**utput **C**ompare / **I**nput **C**apture **R**egister | 비교값(주기) / 캡처값 |
| **CNT** | **C**ou**NT**er | 현재 카운터 (RO) |
| **CLKSRC** | **CL**oc**K** **S**ou**RC**e | 카운터 클럭 소스 (001=ipg, 101=24M osc) |
| **FRR** | **F**ree-**R**un / **R**estart | 0=Restart(OCR1 도달 시 0복귀→주기) / 1=Free-Run(끝까지 셈) |
| **EN / ENMOD / SWR** | **EN**able / **EN**able **MOD**e / **S**oft**W**are **R**eset | 시작 / EN 시 0부터 / 소프트리셋 |
| **OF1** | **O**utput compare **F**lag 1 | OCR1 매칭 플래그 (W1C) |

## 8. RTWDOG — Watchdog / AUTOSAR Wdg (Ch.53)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **RTWDOG** (WDOG3) | **R**eal-**T**ime **W**atch**DOG** | SW 가 안 쓰다듬으면 칩 리셋 (deadman switch) |
| **CS** | **C**ontrol and **S**tatus | 제어 + 상태 통합 레지스터 |
| **CNT** | **C**ou**NT**er | 쓸 때는 명령(refresh/unlock) 레지스터, 읽으면 카운터 |
| **TOVAL** | **T**ime**O**ut **VAL**ue | 타임아웃 값 (16비트 유효, 도달 시 리셋) |
| **WIN** | **WIN**dow | window mode 하한 (너무 일찍 refresh도 리셋) |
| **EN / CLK / UPDATE / INT / PRES** | **EN**able / **CL**oc**K** / — / **INT**errupt / **PRES**caler | 켜기 / 클럭 / 재설정허용 / 리셋전 인터럽트 / 256분주 |
| **CMD32EN** | **C**o**M**man**D** **32**-bit **EN**able | refresh/unlock 을 32비트 단일 write 로 |
| **RCS / ULK** | **R**e**C**onfiguration **S**uccess / **U**n**L**oc**K** | (RO) 재설정 성공 / 잠금 풀림 |
| **FLG** | **FL**a**G** | 인터럽트 플래그 (W1C) |
| **refresh / unlock** | — | 카운터 0복귀(0xB480_A602) / write-once 풀기(0xD928_C520) |
| **wdog3_rst_b** | wdog3 reset (active-low) | SRSR bit7 — 워치독이 일으킨 리셋이면 1 |
| **deadman switch** | — | 손 떼면 멈추는 안전장치 비유 |

## 9. QuadTimer — TMR / AUTOSAR Icu (Ch.49)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **TMR / QuadTimer** | **T**i**M**e**R** | 채널 4개 타이머. 입력 신호 측정(Icu) |
| **Icu** | **I**nput **C**apture **U**nit | 외부 신호 측정 드라이버 (GPT 의 거울상) |
| **COMP1/2 / CAPT** | **COMP**are / **CAPT**ure | 비교값 / 입력 엣지에 CNTR 래치 |
| **LOAD / CNTR** | — / **C**ou**NT**e**R** | 카운터 재초기화 값 / 현재 카운터 |
| **CTRL / SCTRL** | **C**on**TR**o**L** / **S**tatus and **C**on**TR**o**L** | 카운트 모드 설정 / 캡처·상태 |
| **CM / PCS / SCS** | **C**ount **M**ode / **P**rimary / **S**econdary **C**ount **S**ource | 카운트 방식 / 1·2차 카운트 소스 (PCS 에 "Counter N 출력" 있음=cascade) |
| **OUTMODE / OFLAG** | **OUT**put **MODE** / **O**utput **FLAG** | 출력 동작(011=토글→사각파) / 출력 신호 |
| **CAPTURE_MODE / IEF / IPS / OEN** | — / **I**nput **E**dge **F**lag / **I**nput **P**olarity **S**elect / **O**utput **EN**able | 캡처 엣지 선택 / 캡처 플래그 / 입력 극성 / 출력핀 enable |
| **cascade** | — | 한 채널이 다른 채널의 출력을 내부에서 카운트 (부품 없이 신호 측정) |

## 10. AUTOSAR

| 약어 | 풀이 | 설명 |
|---|---|---|
| **AUTOSAR** | **AUT**omotive **O**pen **S**ystem **AR**chitecture | 자동차 SW 표준 아키텍처 |
| **MCAL** | **M**icrocontroller **A**bstraction **L**ayer | 하드웨어 직접 다루는 최하위 드라이버 계층 |
| **facade** | — | register 드라이버 위에 얹은 표준 API 층 (예: Spi.c → lpspi.c) |
| **Spi/Adc/Pwm/Can/Gpt/Wdg/Icu** | — | MCAL 표준 드라이버 ↔ 본 프로젝트의 LPSPI/ADC/PWM/CAN/GPT/RTWDOG/QuadTimer |
| **Mcu/Port/Dio** | **M**icro**C**ontroller **U**nit / — / **D**igital **I**nput **O**utput | 클럭(CCM)/핀(IOMUXC)/GPIO MCAL 드라이버 |
| **Std_ReturnType** | **St**an**d**ard Return Type | AUTOSAR 표준 반환형(E_OK/E_NOT_OK) |
| **CanIf / WdgM** | **Can** **I**nter**f**ace / **Wdg** **M**anager | CAN 스택 상위 / 워치독 alive 감시 상위 |
| **ISO 26262** | — | 자동차 기능안전 표준 (Wdg/WdgM 이 기본 빌딩블록) |

### AUTOSAR Can 스택 용어 (Can_Write facade)

| 약어 | 풀이 | 설명 |
|---|---|---|
| **PDU** | **P**rotocol **D**ata **U**nit | 프로토콜의 데이터 단위 = 헤더(ID/DLC) + 페이로드. 한 프레임 분의 정보 묶음. `Can_PduType`(id/length/sdu)이 이것 |
| **HOH** | **H**ardware **O**bject **H**andle | 하드웨어 메시지 오브젝트(=CAN 메시지버퍼/메일박스)를 가리키는 추상 핸들. "몇 번 MB냐"를 지칭 |
| **Hth** | **H**ardware **T**ransmit **H**andle | HOH 중 **송신용** — TX 메일박스. `Can_Write(Hth, PduInfo)` 의 그 인자 (레벨1: Hth=0 → MB0) |
| **Hrh** | **H**ardware **R**eceive **H**andle | HOH 중 **수신용** — RX 메일박스 (Hth 의 짝) |
| **sdu / SDU** | **S**ervice **D**ata **U**nit | PDU 의 페이로드(데이터 본문). `Can_PduType.sdu` = 데이터 포인터 |
- 상위 CanIf 가 논리 PDU ↔ 하드웨어 오브젝트(HOH)를 매핑 → `Can_Write` = "이 PDU 를(PduInfo) 이 슬롯으로(Hth) 보내라". 컨트롤러별 MB 차이를 HOH 로 감춰 이식성 확보 (register `Can1_Send` 위에 얹은 이유).
