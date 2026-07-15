# FlexSPI / NOR flash 정리 — Fls 챕터의 지도

> 참조: RM Rev.2 **Ch.27 (FlexSPI Controller)** + NOR flash datasheet (칩은 F-2 에서 JEDEC ID 로 확정)
> 요약: 우리 펌웨어는 이미 FlexSPI 위에서 돌고 있다(XIP). 이 챕터는 같은 컨트롤러의
> **또 다른 문(IP 명령)** 을 열어, flash 를 읽고 지우고 쓰는 드라이버를 만든다.

---

## Part 1. 큰 그림 — 같은 flash 로 통하는 두 개의 문

FlexSPI 컨트롤러에는 flash 에 접근하는 경로가 두 개 있다.

```
                        ┌──────────── FlexSPI ────────────┐
CPU 가 0x60000000 읽기 ──▶ AHB 경로 ──┐                    │
   (XIP: 명령 fetch,      "메모리인 척" │   LUT (명령 악보) ──▶ QSPI NOR flash
    상수 읽기 — 자동)                  │                    │      (8MB)
레지스터로 명령 조립  ────▶ IP 명령 경로─┘                    │
   (ID 읽기, erase,       "명령 한 건씩"                     │
    program — 수동)      └──────────────────────────────────┘
```

- **AHB 경로**: CPU 가 0x6000_0000 번지를 읽으면 FlexSPI 가 알아서 flash 읽기 명령을 만들어
  데이터를 가져온다. flash 가 메모리처럼 보이는 이유이고, **우리 XIP 부팅이 바로 이 문 위에서
  돌고 있다.** 부팅 때 ROM 이 읽어 가는 FCB(shared/flexspi_nor_config.c)가 이 경로의 설정이다.
- **IP 명령 경로**: 레지스터에 "주소는 여기, 시퀀스는 이것, 크기는 이만큼"을 적고 트리거를
  당기면 명령이 **한 건** 나간다. JEDEC ID 읽기, erase, program 처럼 "읽기 아닌 일"은 전부
  이 문으로 한다. **이 챕터에서 만드는 것이 이 경로의 드라이버다.**

두 경로는 하나의 LUT 와 하나의 flash 를 공유한다 — 그래서 편리하고, 그래서 위험하다
(Part 5 안전 규칙).

---

## Part 2. LUT — 명령의 악보

FlexSPI 는 `0x9F` 같은 opcode 를 레지스터에 직접 쓰는 구조가 아니다. **LUT(look-up table)에
"이 명령은 이런 순서로 신호를 내보낸다"는 악보를 미리 적어 두고, 실행할 때는 악보 번호만
지정한다.** SPI/QSPI/멀티라인이 뒤섞인 프로토콜을 하나의 틀로 다루기 위한 설계다.

### 구조

```
LUT = 32-bit 워드 64개 (offset 0x200~)
시퀀스 1개 = 워드 4개 = 인스트럭션 최대 8개  →  시퀀스 슬롯 총 16개 (0~15)
인스트럭션 1개 = 16-bit:
  [15:10] OPCODE   무엇을 (명령 보내기/주소 보내기/더미/읽기/쓰기/정지)
  [9:8]   NUM_PADS 몇 가닥으로 (1/2/4 라인)
  [7:0]   OPERAND  값 (opcode 바이트, 주소 비트 수, 더미 사이클 수 ...)
```

주요 OPCODE: `CMD_SDR`(0x01, 명령 바이트 송신) · `RADDR_SDR`(0x02, 주소 송신) ·
`DUMMY_SDR`(0x0C, 더미 사이클) · `READ_SDR`(0x09, 데이터 수신) · `WRITE_SDR`(0x08, 데이터 송신) ·
`STOP`(0x00, 시퀀스 끝).

### 이미 쓰고 있는 악보 해부 — FCB 의 read 시퀀스

우리 FCB 에 들어 있는 시퀀스 0번(부팅 read)은 이렇게 생겼다:

```c
/* READ: 0xEB, 1-4-4 (shared/src/flexspi_nor_config.c) */
[0] = FLEXSPI_LUT_SEQ(CMD_SDR,   PAD_1, 0xEB,  RADDR_SDR, PAD_4, 0x18),
[1] = FLEXSPI_LUT_SEQ(DUMMY_SDR, PAD_4, 0x06,  READ_SDR,  PAD_4, 0x04),
```

읽으면: "명령 0xEB 를 1가닥으로 → 주소 24비트(0x18)를 4가닥으로 → 더미 6사이클 →
데이터를 4가닥으로 수신." 이것이 Quad I/O Read 이고, 지금 이 순간에도 CPU 의 명령 fetch 가
이 악보로 flash 를 읽고 있다. **F-2 에서 우리가 할 일은 같은 문법으로 `0x9F`(JEDEC ID) 악보를
빈 슬롯에 하나 더 적는 것뿐이다.**

### LUT 는 잠겨 있다

LUT 수정은 실수 방지 잠금 뒤에 있다: `LUTKEY`(0x18)에 열쇠값 **0x5AF05AF0** 을 쓰고
`LUTCR`(0x1C)로 unlock → 수정 → lock. RTWDOG 의 unlock 매직값과 같은 철학이다.

---

## Part 3. IP 명령 실행 절차 (레지스터 지도)

FLEXSPI base = **0x402A_8000** (RM Ch.27 memory map).

| 오프셋 | 레지스터 | 역할 |
|---|---|---|
| 0x00 | MCR0 | 모듈 제어 (리셋/enable — **우리는 건드리지 않는다**, Part 5) |
| 0x14 | INTR | 상태 플래그. `IPCMDDONE` 을 W1C 폴링 |
| 0x18 / 0x1C | LUTKEY / LUTCR | LUT 잠금 해제 (키 0x5AF05AF0) |
| 0xA0 | IPCR0 | 명령이 향할 **flash 주소** (SFAR) |
| 0xA4 | IPCR1 | **시퀀스 번호**(ISEQID) + **데이터 크기**(IDATSZ) |
| 0xB0 | IPCMD | 트리거 (TRG) — 방아쇠 |
| 0xB8 | IPRXFCR | RX FIFO 제어 (클리어/watermark) |
| 0xE0 | STS0 | 시퀀스 실행 상태 |
| 0x100~ | RFDR0~31 | RX FIFO — 수신 데이터를 여기서 읽음 |
| 0x180~ | TFDR0~31 | TX FIFO — 송신 데이터를 여기에 씀 |
| 0x200~ | LUT[0~63] | 명령 악보 (Part 2) |

실행 절차는 다섯 줄이다 (RM 27장 "IP command" 프로그래밍 모델):

```
① IPCR0  = flash 주소            (ID 읽기처럼 주소가 없는 명령은 0)
② IPCR1  = 시퀀스 번호 | 데이터 크기
③ IPCMD  = TRG                   (발사)
④ INTR[IPCMDDONE] 폴링 → W1C     (완료 대기 — LPSPI 의 TCF 와 같은 역할)
⑤ RFDR 에서 수신 데이터 읽기      (읽기 명령일 때)
```

bring-up 5단계 공통 패턴과의 관계: **클럭(①)과 핀(②)은 이미 켜져 있다** — 부팅 자체가
FlexSPI 를 쓰는 중이니까. 그래서 이 챕터는 곧바로 설정(LUT)→사용(IP 명령)에 집중한다.
지금까지 모든 드라이버와 반대로, "이미 돌아가는 모듈에 얹혀 작업"하는 것이 특징이다.

---

## Part 4. NOR flash 라는 물건 (메모리 반도체 관점)

여기부터가 이 챕터의 진짜 학습 대상이다. 지금까지의 페리페럴과 달리 flash 는 **저장 매체의
물리적 성질**이 인터페이스에 그대로 드러난다.

### 비대칭성 — 읽기는 자유, 쓰기는 조건부

```
읽기(read)      : 아무 주소나, 바이트 단위, 자유롭게
쓰기(program)   : 비트를 1 → 0 으로만 바꿀 수 있다. 페이지(256B) 단위
지우기(erase)   : 0 → 1 로 되돌리는 유일한 방법. 섹터(4KB) 단위 통째로
```

"수정"이라는 연산이 없다. 한 바이트를 고치려면 섹터 4KB 를 지우고 다시 써야 한다 —
**erase-before-write**. 이 비대칭이 FTL, wear-leveling, Fee(EEPROM 에뮬레이션) 같은
상위 계층이 존재하는 이유다.

### 시간 스케일 — 왜 상태 폴링과 비동기 API 가 필수인가

| 연산 | 대략적 시간 | 비고 |
|---|---|---|
| 읽기 | ~µs | AHB 로는 체감 즉시 |
| Page program (256B) | ~1ms 미만 | 내부 고전압 펌핑 |
| Sector erase (4KB) | ~수십 ms | 읽기의 수만 배 |
| Chip erase (8MB) | ~수십 초 | pyocd 플래시가 오래 걸리던 이유 |

erase/program 을 명령으로 보내고 나면 flash 는 **내부적으로 바쁘다** — status 레지스터의
**WIP**(Write In Progress) 비트가 끝날 때까지 1 이다. 그래서 ① WIP 폴링이 필수이고,
② ms~수십 ms 를 블로킹으로 기다릴 수 없어서 MCAL Fls 가 **비동기 job 모델**(F-5)인 것이다.

### 실수 방지 잠금 — WEL

모든 쓰기 계열 명령(erase/program) 앞에는 **WREN**(0x06, Write Enable)을 먼저 보내
**WEL**(Write Enable Latch) 비트를 세워야 한다. 한 번 쓰면 자동으로 풀린다.
"살아있음의 증명을 어렵게 만든" 워치독 매직값과 같은 계열의 안전장치 — 폭주한 코드가
우연히 erase 를 쏘기 어렵게 만든 이중 방아쇠다.

### 예상 명령 세트 (칩 확정은 F-2 의 몫)

기존 기록(secure boot 챕터)에는 **Winbond W25Q64JV** 로 남아 있는데, EVK 에 ISSI 칩이
장착된 개체도 있어서 **F-2 에서 JEDEC ID(0x9F)를 직접 읽어 확정**한다 — 첫 구현이 곧 검증.

| 명령 | opcode | 용도 |
|---|---|---|
| JEDEC ID | 0x9F | 제조사/용량 확인 (Winbond = `EF 40 17`) |
| Read Status-1 | 0x05 | WIP(bit0), WEL(bit1) |
| Write Enable | 0x06 | WEL 세우기 |
| Read Data | 0x03 / 0x0B | 1라인 읽기 (느긋한 검증용) |
| Sector Erase 4KB | 0x20 | F-4 |
| Page Program | 0x02 | F-4 (256B 단위) |

---

## Part 5. 안전 규칙 — XIP 위에서 flash 를 만진다는 것

이 챕터의 모든 코드는 **자기가 딛고 선 땅을 파는 작업**이다. 규칙을 먼저 세운다.

1. **MCR0 리셋·클럭 변경 금지.** FlexSPI 를 리셋하는 순간 명령 fetch 가 끊겨 즉사한다.
   기존 설정(FCB 가 만든)에 얹혀서만 작업한다.
2. **부팅 LUT 시퀀스(0번대)는 읽기 전용.** 우리 악보는 빈 슬롯에만 적는다.
3. **erase/program 중 flash 는 읽기를 못 준다** (F-4 의 고비). 그 순간 코드 fetch 도 불가능
   → 해당 구간 코드는 **RAM(ITCM)에서 실행**해야 하고, flash 를 읽을 수 있는 인터럽트/
   워치독도 관리해야 한다. I-cache 사건에서 배운 "실행 경로가 어디를 딛고 있는가"라는
   질문이 여기서 정면으로 돌아온다.
4. **실험 영역은 이미지 밖.** 우리 이미지는 0x6000_0000 부터 수십 KB — 실험은 flash 끝쪽
   미사용 섹터에서만 한다. 접근 허용 영역은 **Fls config 로 제한**하고 위반은 DET 신고
   (post-build Config 패턴의 실전 재활용).
5. F-2(ID)·F-3(status/read)는 읽기 전용이라 안전하다. 위 규칙이 전부 발동하는 것은 F-4 부터.

---

## Part 6. 다음 단계 연결

- **F-2 (JEDEC ID)**: LUT 빈 슬롯에 `0x9F` 시퀀스 등록 → IP 명령 5단 절차 → RFDR 에서
  3바이트. 합격 기준 = 제조사 ID 로 칩 확정 (`EF 40 17` = W25Q64JV / `9D ..` = ISSI).
  쓰기 없음 — 부팅에 영향을 줄 수 있는 요소가 전혀 없는 first blood.
- F-3: status(0x05)/read(0x03) — WIP/WEL 관찰 습관 만들기.
- F-4: WREN → sector erase → page program → 리부팅 지속성. RAM 함수 도전.
- F-5: MCAL Fls facade — 비동기 job 모델 (`MEMIF_JOB_PENDING`, `Fls_MainFunction`).
