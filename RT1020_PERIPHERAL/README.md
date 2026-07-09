# i.MX RT1020 Bare-metal MCAL Drivers

> HAL / SDK / 외부 라이브러리 **없이** Reference Manual 만 보고 i.MX RT1020 (Cortex-M7) 의
> 주요 페리페럴 7종을 **register-level 로 직접** 올리고, 그 위에 **AUTOSAR MCAL 표준 API** 를 얹은 베어메탈 프로젝트입니다.
> ROM 부팅 체인부터 시작해, 각 드라이버를 보드에서 **실물 검증**하며 쌓아 올렸습니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![drivers](https://img.shields.io/badge/MCAL_drivers-7-orange)]()
[![layer](https://img.shields.io/badge/register→IRQ→DMA→AUTOSAR-4_layer-brightgreen)]()
[![deps](https://img.shields.io/badge/HAL%2FSDK_deps-0-success)]()

---

## 한눈에

AUTOSAR set 개발 본업의 **MCAL(Microcontroller Abstraction Layer)** 을 베어메탈에서 register-level 로 재구현합니다.
추상화 계층 없이 RM 의 레지스터 시퀀스를 직접 다뤄, **"신호가 핀으로 나가기까지 무엇이 어떤 순서로 일어나는가"** 를 끝까지 따라갑니다.

- **HAL / SDK 를 쓰지 않습니다.** CCM · IOMUXC · 각 페리페럴 레지스터를 RM 보고 직접 세팅.
- **7개 MCAL 드라이버** 를 register-level 로 구현하고, 각각 **AUTOSAR 표준 API 파사드** 로 감쌌습니다.
- **CPU 오프로드 사다리** 를 SPI 로 실증: `polling → IRQ → DMA` 를 DWT 사이클 카운터로 정량 비교.
- **부품 없이 실물 검증**: CAN loopback 송수신 · 워치독 refresh/타임아웃 리셋 · GPT 주기 인터럽트 · QuadTimer cascade.
- **실무 MCAL 인프라 3종 재현**: DET(개발 에러 신고) · post-build 스타일 설정 테이블 · **WdgM alive supervision** — 파사드 위에 표준 BSW 패턴까지 얹었습니다.
- **XIP 캐시 트러블슈팅**: "코드 몇 줄로 증상이 토글되는" 워치독 Heisenbug 를 근본원인(I-cache 미활성)까지 추적·수정 → [트러블슈팅 문서](docs/TROUBLESHOOTING_ICACHE_WATCHDOG.md).

---

## 구현한 MCAL 드라이버

| AUTOSAR | RT1020 IP (RM Ch.) | register 드라이버 핵심 | 무도구 검증 |
|---|---|---|---|
| **Spi** | LPSPI (Ch.43) | CCM→IOMUXC→CR/CFGR1/CCR/TCR/FCR, poll/IRQ/DMA | TCF + 3단 사이클 비교 |
| **Adc** | ADC (Ch.61) | 캘리브레이션 → CFG → HC0 → COCO → R0 | VREFSH 내부채널 ≈ full-scale |
| **Pwm** | eFlexPWM (Ch.50) | SM0 INIT/VAL1-3 + OUTEN + MCTRL(LDOK/RUN) | SM0CNT 진행 + duty(VAL3) |
| **Can** | FlexCAN (Ch.40) | Freeze→비트타이밍/LPB→MB → CODE=TX/EMPTY/FULL | **loopback TX→RX 데이터 일치** |
| **Gpt** | GPT (Ch.47) | CLKSRC/FRR(Restart) + OCR1 + OF1 IRQ | 주기 인터럽트 tick 증가 |
| **Wdg** | RTWDOG (Ch.53) | unlock→CS/TOVAL, refresh(매직값), SRC_SRSR | **refresh=생존 / 중단=리셋** |
| **Icu** | QuadTimer (Ch.49) | ch0 사각파 → ch1 cascade 카운트(PCS) | 엣지 카운트 증가 |

+ **Mcu / Port / Dio** (CCM / IOMUXC / GPIO) 파사드.

---

## 4계층 아키텍처 (SPI 예시)

한 드라이버를 **네 단계로 점진 구현**하며 각 층의 트레이드오프를 실측했습니다.

```
[ 응용 ]   main.c — Spi_* 표준 API 만 호출
              │
[ facade ]  Spi.c      ← AUTOSAR: Spi_SyncTransmit / Spi_AsyncTransmit / SetAsyncMode
              │
[ register] lpspi.c    ← LPSPI1_Send_Byte / _IRQ / _DMA (poll·인터럽트·DMA 3구현)
              │
[ HW ]      LPSPI1 레지스터 (CR/CFGR1/CCR/TCR/FCR/DER ...)
```

CPU 오프로드 정량 비교 (DWT CYCCNT, kick 이후 main 자유 사이클):

| 방식 | 전송 중 CPU 점유 | 인터럽트 횟수 |
|---|---|---|
| polling | 전송 내내 묶임 (~10만 cyc) | 0 |
| IRQ | watermark 때만 깨움 | 수 회 |
| DMA | kick 후 자유, 완료 1회만 | 1 |

나머지 6개 드라이버도 같은 방식으로 register 층 위에 표준 API 파사드를 얹었습니다.
모듈마다 API 모양이 달라(`Pwm_SetDutyCycle` / `Adc_ReadGroup`(그룹+버퍼) / `Can_Write`(HOH+PDU) / `Wdg_SetTriggerCondition`) — 그 차이를 직접 구현한 것이 학습 포인트입니다.

---

## AUTOSAR 확장 — 실무 MCAL 인프라 3종

파사드 완성 후, 실제 MCAL 코드의 "모양"을 결정하는 세 가지 표준 패턴을 SWS 기준으로 재현했습니다.

| 패턴 | 구현 | 실증 |
|---|---|---|
| **DET** (Default Error Tracer) | 7개 모듈 API 입구에 UNINIT/PARAM/상태위반 guard + `Det_ReportError` 신고 (컴파일 스위치로 양산 빌드에서 제거 가능) | 의도적 오호출 → `mod/api/err` 신고를 UART 로 수신. main 의 `Can_Init(0)` 누락 실수를 신고 로그만으로 30초 진단 |
| **Post-build Config** | 모든 `Xxx_Init(const Xxx_ConfigType*)` 가 `src/Cfg.c` 의 설정 테이블을 참조 — 드라이버와 설정의 물리적 분리 | **Cfg.c 만 수정**해 Gpt tick·Icu 엣지레이트 2배, SPI SCK 1MHz→514kHz 를 보드에서 실측 (드라이버 .c 무수정) |
| **WdgM** (alive supervision) | 응용은 체크포인트 증언만, GPT tick ISR 이 창당 보고 횟수를 판정해 범위 밖이면 굶겨 리셋 | hang(침묵→FAILED→EXPIRED→리셋)과 **폭주(과다 보고→FAILED)** 를 모두 검출 — 직접 refresh 로는 불가능한 감시 |

각 단계가 다음 단계의 부품이 됩니다: DET 의 상태변수 → Config 의 NULL/재Init 거부 → WdgM 의 판정 기반.
상세: [DET](docs/DET_NOTES.md) · [Config](docs/CONFIG_NOTES.md) · [WdgM](docs/WDGM_NOTES.md)

---

## 트러블슈팅 하이라이트 — 워치독 무한 리셋 = I-cache/XIP 문제

Gpt 파사드를 추가하자 워치독이 무한 리셋. 워치독 코드는 멀쩡했고, 범인은 **I-cache 가 꺼진 채 flash XIP 실행**이라 `delay_busy` 루프의 실행시간이 **코드 주소 정렬에 의존**한 것이었습니다(둘째 delay 루프가 fetch-line 경계에 걸쳐 >2초 → 타임아웃).

여러 오답(타이밍/LED/reconfig 윈도우/GPT IRQ)을 **하드웨어 계측으로 반증**하고, `main` 진입 전 **I-cache enable** 로 결정적 실행을 확보해 해결. 전 과정을 [docs/TROUBLESHOOTING_ICACHE_WATCHDOG.md](docs/TROUBLESHOOTING_ICACHE_WATCHDOG.md) 에 포스트모템으로 정리했습니다.

---

## 시작하기

```bash
./build.sh        # arm-none-eabi-gcc 로 단일 이미지 빌드 (build/rt1020_peripheral.{elf,bin})
./flash_mcu.sh    # pyocd 로 chip erase + 0x60000000 flash (-t mimxrt1020)

# 시리얼 모니터 (별도 터미널)
gtkterm -p /dev/ttyACM0 -s 115200
```

기대 출력 (요약):

```
[Spi] poll cyc = 0x0001A...  / irq cyc = 0x00..  / dma cyc = 0x00..
[Det] mod=0x7B api=0x04 err=0x0A/15/14   # 의도적 오호출 3건 신고 (DET 데모)
[Adc] ReadGroup = OK   VREFSH R0 = 0x00000FF4
[Pwm] duty 25% set, VAL3 = 0x00003FFF
[Can] loopback = OK (TX -> RX data match)   write(HOH=0) = OK
[Gpt] CNT advancing = YES
[Icu] CNTR advancing = YES
[Mcu] reset SRSR = 0x00000001            # 리셋 원인 (2차 부팅에선 0x80 = watchdog)
[PERI] beat 0x.. / [Gpt] tick = 0x.. / [WdgM] status = 0x00
  # 1차 부팅: beat 5 부터 체크포인트 침묵 → status 0→1(FAILED)→2(EXPIRED) → 리셋
  # 2차 부팅: 워치독 리셋 감지 → 증언 지속 → 영구 생존
```

---

## 문서

- **[docs/GLOSSARY.md](docs/GLOSSARY.md)** — 프로젝트 전체 약어 (레지스터/비트/AUTOSAR)
- **[docs/TROUBLESHOOTING_ICACHE_WATCHDOG.md](docs/TROUBLESHOOTING_ICACHE_WATCHDOG.md)** — 워치독/I-cache 근본원인 추적 포스트모템
- AUTOSAR 확장 노트 (SWS 개념 · 설계 · 실측 결과):
  [DET](docs/DET_NOTES.md) · [Post-build Config](docs/CONFIG_NOTES.md) · [WdgM](docs/WDGM_NOTES.md)
- RM 정리 노트 (드라이버별 레지스터 맵 · 시퀀스 · 검증 기준):
  [LPSPI](docs/LPSPI_NOTES.md) · [ADC](docs/ADC_NOTES.md) · [PWM](docs/PWM_NOTES.md) ·
  [CAN](docs/CAN_NOTES.md) · [GPT](docs/GPT_NOTES.md) · [RTWDOG](docs/WDG_NOTES.md) · [QuadTimer](docs/ICU_NOTES.md)

---

## 프로젝트 구조

```
RT1020_PERIPHERAL/
├── CMakeLists.txt              # 단일 이미지 빌드 (IS_BOOTLOADER 분기 = flash XIP)
├── build.sh / flash_mcu.sh
├── linker/peripheral.ld        # 메모리 맵 (FCB/IVT/FLASH/DTCM/ITCM)
├── include/                    # AUTOSAR facade 헤더 (Spi/Adc/Pwm/Can/Gpt/Wdg/Icu, Std_Types ...)
├── src/
│   ├── main.c                  # 7개 드라이버 데모 하네스 + 표준 API 호출
│   ├── Spi.c  / lpspi.c        # SPI facade / register (4계층 모범)
│   ├── Adc.c Pwm.c Can.c Gpt.c Wdg.c Icu.c   # register + facade (모듈당 1파일)
│   ├── Mcu.c Port.c Dio.c      # CCM/IOMUXC/GPIO 파사드 (+ Mcu_GetResetReason)
│   ├── Det.c / Cfg.c / WdgM.c  # AUTOSAR 확장: 에러 신고 / 설정 테이블 / alive supervision
│   └── boot_header.c           # IVT + boot_data (ROM 부팅 헤더)
├── shared/                     # 검증된 공통 plumbing (startup, clock, uart, led, regs, cache ...)
└── docs/                       # RM 정리 노트 + 용어집 + 트러블슈팅
```

---

## 안 쓴 이유 / 한계

- **HAL/SDK 미사용**: 베어메탈 + 레지스터 원리 학습이 목적. 실무 양산은 검증된 HAL 권장.
- **AUTOSAR 는 축소 재현**: 표준 API 모양 + DET/설정 테이블/WdgM 까지 구현했지만, 도구 생성 설정(EB tresos)·전체 상태머신·Spi 의 Job/Sequence 계층·DEM 연동은 범위 밖.
- **외부 device 검증은 loopback/내부채널로 대체**: EVK 헤더 unpopulated 라 SPI 실device·ADC 외부 pot·Icu 실캡처는 핀헤더/로직애널라이저 확보 시 확장 예정.
