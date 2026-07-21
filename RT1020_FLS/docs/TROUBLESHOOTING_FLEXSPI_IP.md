# FlexSPI IP 명령 트러블슈팅 기록 (F-2 ~ F-3)

> JEDEC ID·status·read 세 명령을 IP 명령 경로로 만들면서 겪은 실패와 그 추적 과정.
> 결론부터: **세 번의 실패 중 두 번이 LUT 인코딩 오타였고, 그것을 잡아낸 것은
> 열 번의 가설이 아니라 레지스터 덤프 한 줄이었다.**

---

## Part 0. 왜 이 기록을 남기는가

F-2/F-3 는 코드량으로 200줄 남짓인데 디버깅에 여러 라운드가 걸렸다. 원인은 실력이
아니라 **방법**이었다 — 증상을 보고 가설을 세워 코드를 고치고 다시 굽는 루프를 반복했다.
하드웨어를 직접 들여다보지 않은 채로.

정작 문제를 끝낸 것은 `LUT[8]`, `INTR`, `IPRXFSTS`, `RFDR0` 네 값을 UART 로 찍어본
**단 한 번의 덤프**였다. 이 기록의 목적은 그 교훈을 F-4 이후에 다시 쓰기 위함이다.

---

## Part 1. 사례 1 — JEDEC ID read FAILED (cmd error)

### 증상

```
[FLS] JEDEC ID read FAILED, status=0x00000002  (1=timeout, 2=cmd error)
[FLS] Status-1 = 0x42  WIP=0 WEL=1
[FLS] IP read  @0x000000 = 0x424643C6
[FLS] AHB read @0x60000000 = 0x42464346
[FLS] READ MISMATCH
```

전에 잘 되던 JEDEC ID 가 갑자기 실패했고, **뒤따르는 status·read 까지 값이 이상**했다.

### 추적

F-3 를 추가하면서 `FlexSPI_InstallLut()` 을 편집했는데, **JEDEC(seq7) 설치 블록이
통째로 사라지고** status/read 만 남아 있었다. 부팅 직후 하드웨어 LUT 는 ROM 이 FCB 에서
복사한 것뿐이라 FCB 에 없는 **seq7 은 전부 0(= 즉시 STOP)** 이다. 빈 악보를 트리거하니
`IPCMDERR` 이 떴다.

### 진짜 무서운 부분 — 실패한 명령의 잔재

문제는 JEDEC 하나로 끝나지 않았다는 것이다. AHB(XIP)는 멀쩡히 `0x42464346` 을 읽는데
IP read 만 첫 바이트가 깨졌다. **에러 난 명령이 컨트롤러를 error 상태로 남겨 뒤의
명령까지 오염**시킨 것이다.

### 교훈

- **실패한 명령을 방치하면 다음 명령을 갉아먹는다.** 에러 경로에서도 FIFO/플래그를
  반드시 정리해야 한다.
- **AHB(XIP)와 IP 경로를 나란히 찍어두면 범위가 즉시 좁혀진다.** XIP 가 정상인데
  IP 만 깨졌다면 flash 도 배선도 아닌 **우리 IP 명령 경로**가 범인이다.

---

## Part 2. 사례 2 — status 가 직전 명령의 값을 되읽음

### 증상

```
[FLS] JEDEC ID = 9D 60 17  vendor=ISSI
[FLS] Status-1 = 0x9D  WIP=1 WEL=0      ← 0x9D 은 방금 JEDEC 이 읽은 제조사 바이트!
```

status 가 진짜 status 가 아니라 **직전 JEDEC 명령의 첫 바이트**를 그대로 돌려줬다.

### 잘못 짚은 가설들 (기록 목적)

| 가설 | 결과 |
|---|---|
| RX FIFO 잔상 — `CLRIPRXF` 가 안 먹는다 | **부분 정답**. FIFO 는 실제로 pop 되지 않았지만 근본 원인은 아니었다 |
| 시퀀스 명령 포인터 오염 — `FLSHCR2[CLRINSTRPTR]` 필요 | **기각.** RM: *"For IP command ... FlexSPI controller always executes from instruction pointer 0."* IP 명령은 항상 instruction 0 부터 시작한다 |

### 중간 소득 — RX FIFO 읽기 절차 (RM 27.4.10.1)

가설을 검증하다 RM 에서 실제로 중요한 규칙을 찾았다.

> **IP RX FIFO data is not popped out by each reading access** to IP RX FIFO,
> but popped by writing 0x1 to register `INTR[IPRXWA]` bit.
> ... software should clear IP RX FIFO by setting `IPRXFCR[CLRIPRXF]`.
> **Otherwise, the reading data will be wrong for the next reading command to Flash.**

정리하면:

1. FlexSPI 는 수신 데이터를 **64비트(8바이트) 단위로** IP RX FIFO 에 밀어 넣는다.
   64비트에 못 미치면 0 으로 패딩해서 넣는다.
2. **`RFDR` 을 읽는다고 FIFO 가 비워지지 않는다.** pop 은 `INTR[IPRXWA]` 쓰기로만.
3. 워터마크(기본 8바이트)보다 작은 읽기는 `IPRXWA` 가 안 뜬다 →
   **`IPRXFSTS[FILL]`(0xF0, bits[7:0])을 폴링해 데이터 도착을 확인**한 뒤 읽고,
   읽은 뒤 `IPRXFCR[CLRIPRXF]`(0xB8 bit0)로 비운다.

이 FILL 폴링을 넣자 status 는 잔상을 되읽는 대신 **정직하게 실패**했다 —
"데이터가 오지 않았다"를 정확히 감지한 것이다. 증상이 거짓 성공에서 명시적 실패로
바뀐 것 자체가 진전이었다.

### 진짜 원인 — 레지스터 덤프가 끝냈다

여기서 추측을 멈추고 하드웨어를 찍었다.

```c
UART1_SendString("[DBG] LUT[8]=");   UART1_SendHex32(FLEXSPI_LUT[8]);
UART1_SendString(" INTR=");          UART1_SendHex32(FLEXSPI_INTR);
UART1_SendString(" IPRXFSTS=");      UART1_SendHex32(FLEXSPI_IPRXFSTS);
UART1_SendString(" RFDR0=");         UART1_SendHex32(FLEXSPI_RFDR[0]);
```

```
[DBG] LUT[8]=0x08010405 INTR=0x00000261 IPRXFSTS=0x00000000 RFDR0=0x0017609D
```

한 줄이 전부를 말해줬다.

| 값 | 해석 |
|---|---|
| `LUT[8]=0x08010405` | **status 악보에 READ 명령이 없다** (아래 디코드) |
| `INTR=0x261` | bit0(DONE)=1, bit3(ERR)=0 → **명령은 정상 완료**. 그냥 읽을 게 없었을 뿐 |
| `IPRXFSTS=0` | FILL=0 → 데이터가 FIFO 에 하나도 안 들어옴 |
| `RFDR0=0x0017609D` | 직전 JEDEC 잔상 (`9D 60 17`) |

원인은 status LUT 의 두 번째 명령이 `CMD_READ_SDR`(0x09)이어야 하는데
**`CMD_RADDR_SDR`(0x02)로 적혀 있던 것**이었다. 앞선 편집에서 슬롯이 섞였을 때의
잔재가 그대로 남아 있었다. 데이터 수신 단계가 아예 없으니 FILL 이 0 인 게 당연했다.

수정은 한 단어였다.

```c
/* Before — 두 번째 명령이 "주소 보내기" */
FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x05, CMD_RADDR_SDR, PAD_1, 0x01);

/* After — "데이터 받기" */
FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x05, CMD_READ_SDR,  PAD_1, 0x01);
```

고친 뒤 `LUT[8]=0x24010405`, `IPRXFSTS=0x01`, `RFDR0=0x42` — 전부 정상이 됐다.

---

## Part 3. LUT 워드 디코드 방법 (이걸로 오타를 잡았다)

LUT 값을 읽을 줄 알면 악보가 제대로 심겼는지 눈으로 검증할 수 있다.

### 인코딩 규칙

한 워드에 **16비트 인스트럭션 2개**가 들어간다.

```c
#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1) \
    ( (op0) | ((pad0) << 8) | ((cmd0) << 10) | ((op1) << 16) | ((pad1) << 24) | ((cmd1) << 26) )
```

| 필드 | 비트 | 뜻 |
|---|---|---|
| `op0` | [7:0] | 첫 인스트럭션의 피연산자 |
| `pad0` | [9:8] | 라인 수 (0=1가닥, 2=4가닥) |
| `cmd0` | [15:10] | 첫 인스트럭션의 opcode |
| `op1` | [23:16] | 둘째 인스트럭션의 피연산자 |
| `pad1` | [25:24] | 라인 수 |
| `cmd1` | [31:26] | 둘째 인스트럭션의 opcode |

주요 opcode: `CMD_SDR`=0x01 · `RADDR_SDR`=0x02 · `WRITE_SDR`=0x08 ·
`READ_SDR`=0x09 · `DUMMY_SDR`=0x0C · `STOP`=0x00

### 실제 디코드 예 — 범인을 잡은 계산

```
LUT[8] = 0x08010405
       = 0000 1000 | 0000 0001 | 0000 0100 | 0000 0101

op0  = bits[7:0]   = 0x05          → status 명령 0x05        ✅
pad0 = bits[9:8]   = 0             → 1가닥                    ✅
cmd0 = bits[15:10] = 0x01          → CMD_SDR (명령 보내기)    ✅
op1  = bits[23:16] = 0x01          → 피연산자 1
pad1 = bits[25:24] = 0             → 1가닥
cmd1 = bits[31:26] = 0x02          → RADDR_SDR (주소 보내기)  ❌ READ(0x09)여야 함
```

```
정상값 = 0x24010405   (cmd1 = 0x09 → 0x09 << 26 = 0x24000000)
관측값 = 0x08010405   (cmd1 = 0x02 → 0x02 << 26 = 0x08000000)
```

**최상위 바이트만 보면 빠르다**: `0x24......` 면 둘째 명령이 READ, `0x08......` 면 RADDR.

---

## Part 4. 사례 3 — read 의 첫 바이트만 깨짐

### 증상

status 를 고친 뒤에도 read 만 남았다.

```
[FLS] IP read  @0x000000 = 0x424643C6
[FLS] AHB read @0x60000000 = 0x42464346
```

```
기대 flash[0..3] = 46 43 46 42
읽은 값          = C6 43 46 42
                   ^^ byte0 만, 그것도 0x46 | 0x80 (최상위 비트만 잘못 1)
```

### 추적

바이트 1~3 은 매번 정확한데 **byte0 의 최상위 비트(bit7)만** 틀렸다. SPI 는 MSB 부터
보내므로 bit7 은 **데이터 단계의 첫 비트**이고, 그 순간은 정확히 **주소 전송 → 데이터
수신으로 전환되는 찰나**다.

결정적 대조군이 있었다. **JEDEC(`0x9F`)은 완벽하게 동작했다.** 두 명령의 차이는 하나 —
JEDEC 은 **주소 단계가 없다.** 즉 이 전환 지점을 아예 지나지 않는다.

### 원인과 해법

`0x03`(Normal Read)은 **더미 사이클이 없다.** 주소를 다 내보낸 직후 flash 가 데이터를
몰아주기 시작하는데, 그 왕복 지연 동안 FlexSPI 가 첫 비트를 불안정한 타이밍에 샘플링한다.
두 번째 비트부터는 타이밍이 안정돼 나머지가 다 맞았던 것이다.

해법은 **Fast Read `0x0B`** 다. 주소 뒤에 **더미 8사이클**을 넣어 flash 가 데이터를
안정시킬 시간을 준다.

```c
/* Before — 0x03, 더미 없음 */
FLEXSPI_LUT[4*SEQ + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x03, CMD_RADDR_SDR, PAD_1, 0x18);
FLEXSPI_LUT[4*SEQ + 1] = FLEXSPI_LUT_SEQ(CMD_READ_SDR, PAD_1, 0x04, STOP, PAD_1, 0x00);

/* After — 0x0B Fast Read + 더미 8사이클 */
FLEXSPI_LUT[4*SEQ + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x0B, CMD_RADDR_SDR, PAD_1, 0x18);
FLEXSPI_LUT[4*SEQ + 1] = FLEXSPI_LUT_SEQ(CMD_DUMMY_SDR, PAD_1, 0x08, CMD_READ_SDR, PAD_1, 0x04);
```

> `0x03` 에 더미만 붙이면 안 된다. `0x03` 은 더미를 기대하지 않는 명령이라 데이터가
> 그만큼 밀린다. **더미가 규격에 포함된 `0x0B` 로 명령 자체를 바꿔야 한다.**

이 수정으로 `READ MATCH` 가 떴다.

---

## Part 5. 방법론 — 다음에는 이렇게 한다

### 증상 → 덤프 → 원인 (가설 루프 금지)

IP 명령이 이상하면 **고치기 전에 이 네 값을 먼저 찍는다.**

| 레지스터 | 오프셋 | 무엇을 알려주나 |
|---|---|---|
| `LUT[슬롯 × 4]` | 0x200+ | **악보가 제대로 심겼나** (오타 즉시 발견) |
| `INTR` | 0x14 | bit0 DONE / bit1 GE / bit3 ERR — 명령이 끝났나, 에러인가 |
| `IPRXFSTS` | 0xF0 | FILL[7:0] — 데이터가 실제로 도착했나 |
| `RFDR0` | 0x100 | 받은 데이터 (잔상인지 신선한지) |

이 조합이 실패를 세 갈래로 즉시 분류해준다.

```
악보가 틀림      →  LUT 값이 기대와 다름
명령이 실패      →  INTR 에 ERR/GE 비트
데이터가 안 옴   →  INTR DONE=1 인데 IPRXFSTS FILL=0
잔상을 읽음      →  FILL=0 인데 RFDR 에 옛날 값
```

### 대조군을 만들어라

`0x9F`(주소 없음)가 되고 `0x03`(주소 있음)이 안 됐다는 **대조**가 더미 사이클 원인을
가리켰다. AHB(XIP) 읽기를 IP 읽기와 나란히 찍은 것도 같은 역할을 했다. **되는 것과
안 되는 것의 차이가 곧 원인**이다.

### 정답을 아는 데이터로 검증하라

flash 오프셋 0 에는 FCB 태그 `0x42464346`('FCFB')이 박혀 있다. 이걸 읽으면
**기대값을 이미 알고 있으므로** 한 비트만 틀려도 즉시 보인다. 임의 주소를 읽었다면
`0xC6` 이 틀렸다는 것조차 몰랐을 것이다.

### 한 번에 하나만 바꿔라

status 와 read 를 동시에 고치려다 슬롯이 섞였고, 그게 다시 몇 라운드를 잡아먹었다.
F-4 를 a/b/c 로 쪼갠 이유가 이것이다.

---

## Part 6. 확정된 사실 모음 (F-4 이후에도 유효)

- FlexSPI base `0x402A_8000`. `IPRXFSTS`=0xF0(FILL[7:0], 단위 64비트),
  `IPRXFCR`=0xB8(bit0 `CLRIPRXF`), `RFDR`=0x100, `LUT`=0x200.
- **읽기 명령**: 완료 대기(`IPCMDDONE`) → **FILL 폴링** → `RFDR` 읽기 → `CLRIPRXF` 정리.
- **데이터 없는 명령**(WREN/WRDI): `IDATSZ=0`, **FILL 폴링을 하면 안 된다** —
  오지 않을 데이터를 기다리다 타임아웃난다.
- **주소가 있는 읽기**는 더미 사이클이 필요하다 (`0x0B` + 8사이클).
- IP 명령은 **항상 instruction 0 부터** 실행된다. `FLSHCR2[CLRINSTRPTR]` 은
  AHB read 전용이라 IP 명령과 무관하다.
- 부팅 FCB 가 쓰는 슬롯 **0/1/3/4/5/8/9/11 은 건드리지 않는다.** 우리 악보는
  2(status)/6(read)/7(JEDEC)/10(WREN)/12(WRDI)에 있고 13/14/15 가 남아 있다.

---

## 참고

- FlexSPI IP 명령 절차와 LUT 구조: [FLEXSPI_NOTES.md](FLEXSPI_NOTES.md)
- Status 레지스터 비트 의미: [NOR_STATUS_REGISTER.md](NOR_STATUS_REGISTER.md)
- RM 근거: Ch.27 FlexSPI Controller, 특히 **27.4.10.1 Reading Data from IP RX FIFO**
