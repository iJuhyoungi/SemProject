# RT1020 ECC / RAS — 오류 정정 부호와 메모리 신뢰성

> SECDED(Single Error Correction, Double Error Detection) 부호를 **직접 구현하고**,
> flash 에 **실제로 비트를 뒤집어** 정정과 검출을 실증한 프로젝트입니다. 여기에 주기적
> 검사(patrol scrubbing)까지 얹었습니다. 타깃은 Cortex-M7 기반 NXP i.MX RT1020 입니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![ecc](https://img.shields.io/badge/ECC-SECDED_(13%2C8)_%2B_Hsiao_(72%2C64)-green)]()
[![demo](https://img.shields.io/badge/demo-real_bit--flip_on_flash-red)]()
[![ras](https://img.shields.io/badge/RAS-patrol_scrubbing-orange)]()

---

## 한눈에

메모리는 조용히 틀립니다. 우주선(cosmic ray), 셀 노후, 전압 변동으로 저장된 비트가
뒤집히는데 **아무도 알려주지 않습니다.** 읽으면 그냥 틀린 값이 나옵니다.

ECC 는 데이터에 여분 비트를 붙여 이 문제를 다룹니다. 이 챕터는 그 원리를 패리티부터
쌓아 올립니다.

| 부호 | 능력 | 왜 그런가 |
|---|---|---|
| 패리티 1비트 | 1비트 오류 **검출** | 틀렸다는 것만 알고 어디가 틀렸는지는 모름 |
| 해밍 부호 | 1비트 **정정** | 신드롬이 오류 위치를 가리킴 |
| **SECDED** | 1비트 정정 + 2비트 **검출** | 전체 패리티 1비트를 더해 1비트와 2비트를 구분 |

두 가지 크기로 구현했습니다.

- **(13,8) SECDED** — 데이터 8비트에 검사 5비트. 원리를 손으로 따라갈 수 있는 크기입니다.
- **(72,64) Hsiao SECDED** — 데이터 64비트에 검사 8비트. **실제 DRAM/서버 메모리가 쓰는
  구성**입니다.

---

## API

```c
/* (13,8) SECDED */
uint16_t   Ecc_Encode(uint8_t data);                        /* 8비트 → 13비트 코드워드 */
Ecc_Status Ecc_Decode(uint16_t codeword, uint8_t *dataOut);

/* (72,64) Hsiao SECDED */
void       Ecc72_Init(void);
uint8_t    Ecc72_Encode(uint64_t data);                     /* 64비트 → 검사 8비트 */
Ecc_Status Ecc72_Decode(uint64_t *data, uint8_t ecc);       /* 정정은 제자리에서 */
```

`Ecc_Status` 는 `NO_ERROR` / `CORRECTED` / `UNCORRECTABLE` 세 가지입니다. 정정한 경우와
못 고치는 경우를 **호출부가 구분할 수 있어야** 합니다. 정정은 로그로 남기고, 못 고치면
데이터를 버려야 하기 때문입니다.

---

## 왜 Hsiao 인가

(72,64) 를 만들 때 검사 행렬을 아무렇게나 고를 수 없습니다. Hsiao 부호는 행렬의 각 열이
**홀수 개의 1을 갖도록** 구성하고, 열마다 1의 개수를 최대한 고르게 분배합니다.

이렇게 하면 두 가지를 얻습니다. 1비트 오류와 2비트 오류의 신드롬이 확실히 구분되고,
**XOR 게이트 단수가 줄어 하드웨어 구현이 빨라집니다.** 실제 메모리 컨트롤러가 이 구성을
쓰는 이유입니다.

---

## 시연 — 진짜로 비트를 뒤집습니다

이 챕터에서 가장 공들인 부분입니다. 시뮬레이션이 아니라 **flash 위의 실제 비트**를
조작합니다.

NOR flash 는 **1에서 0으로만 쓸 수 있습니다.** 되돌리려면 sector 를 통째로 지워야 합니다.
그래서 오류 주입은 항상 `1 → 0` 방향입니다. 이 제약이 오히려 현실적입니다 — 실제 셀
열화도 대개 한 방향으로 일어납니다.

```
[ECC] === E-4 flash-backed ECC (real 1->0 bit-flip inject) ===
[ECC]   stored bytes  : ...
[ECC]   clean read    : NO_ERROR : OK
[ECC]   after 1 inject: ...
[ECC]   1-bit error   : CORRECTED, data restored : OK
[ECC]   after 2 inject: ...
[ECC]   2-bit error   : UNCORRECTABLE, detected : OK
```

1비트를 뒤집으면 **정정되어 원래 데이터가 복원**되고, 2비트를 뒤집으면 **정정하지 못하고
검출만** 합니다. SECDED 의 정의가 그대로 눈앞에 나옵니다.

---

## Patrol Scrubbing

정정 가능한 오류(CE)를 방치하면 시간이 지나 **두 번째 비트가 뒤집혀 정정 불가(UE)로
악화**됩니다. 읽을 일이 없는 데이터일수록 위험합니다.

그래서 주기적으로 전체를 훑으며 검사하고, 1비트 오류를 발견하면 **정정한 값으로 다시
기록**합니다. 이것이 patrol scrubbing 이고, 서버·자동차 메모리 시스템의 기본 기능입니다.

```
[ECC] === E-5 patrol scrubbing (CE/UE, flash erase-rewrite) ===
[ECC]   before scrub : ...
[ECC]   scrub: single-bit CE -> erase+rewrite done
[ECC]   after scrub  : ...
[ECC]   post-scrub read: NO_ERROR : OK
[ECC]   CE count = ...
```

flash 에서는 "다시 기록" 이 **erase 후 재기록**이 됩니다. 이 과정에서 전원이 끊기면
데이터를 잃으므로, 실제 제품이라면 원본을 다른 곳에 두고 진행해야 합니다. **정정 가능한
오류를 고치려다 데이터를 잃는 것은 본말전도입니다.**

CE 발생 횟수를 세는 것도 중요합니다. 특정 영역에서 CE 가 반복되면 **셀이 죽어가고 있다는
신호**이고, 실제 시스템은 이때 해당 영역을 격리하거나 교체를 예고합니다.

---

## 진행 단계

| 단계 | 내용 |
|---|---|
| E-0 | 챕터 스캐폴딩 — [RT1020_FLS](../RT1020_FLS/) 의 flash 드라이버 재사용 |
| E-1 | 개념 정리 — 패리티에서 해밍, SECDED 까지 |
| E-2 | (13,8) SECDED 구현 + self-test |
| E-3 | (72,64) Hsiao SECDED 구현 + self-test |
| E-4 | **flash 실제 비트 주입** — 1비트 정정 / 2비트 검출 실증 |
| E-5 | Patrol scrubbing + CE/UE 계수 |
| E-6 | Cheat sheet 로 챕터 봉인 |

---

## 문서

| 문서 | 내용 |
|---|---|
| [ECC_NOTES.md](docs/ECC_NOTES.md) | 패리티에서 SECDED 까지의 원리, 신드롬 계산, Hsiao 행렬 구성 |
| [ECC_CHEATSHEET.md](docs/ECC_CHEATSHEET.md) | 한 장 요약 |
| [FLS_FACADE.md](docs/FLS_FACADE.md) | 재사용한 flash 드라이버 계층 |
| [FLEXSPI_NOTES.md](docs/FLEXSPI_NOTES.md) · [NOR_STATUS_REGISTER.md](docs/NOR_STATUS_REGISTER.md) · [TROUBLESHOOTING_FLEXSPI_IP.md](docs/TROUBLESHOOTING_FLEXSPI_IP.md) | flash 저수준 참고 |

---

## 사용법

```bash
./build.sh
./flash_mcu.sh
```

UART 로 E-2 부터 E-5 까지의 self-test 와 시연 결과가 순서대로 출력됩니다.

---

## 이 챕터가 답하는 것

**"메모리가 조용히 틀리면 어떻게 아는가"** 에 대한 답입니다.

RT1020 자체에는 ECC 하드웨어가 없습니다. 그래서 소프트웨어로 구현했고, 그 과정에서
하드웨어 ECC 컨트롤러가 내부에서 무엇을 하는지 이해하게 됩니다. SSD 펌웨어나 자동차
ECU 처럼 **데이터 신뢰성이 안전과 직결되는 영역**의 기본기입니다.

---

_학습 및 포트폴리오 목적입니다._
