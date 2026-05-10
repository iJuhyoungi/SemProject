# Recovery UART Download Protocol

> B-3 Phase 4 Step 4 의 active recovery 에서 사용하는 UART download protocol.
> Stage 2 가 깨졌을 때 recovery slot 의 application 이 새 Stage 2 binary 를 UART 로 받아 flash 갱신.

---

## 1. 개요

### 목적

Stage 2 의 corruption 을 사용자가 board 분해 / SWD 재flash 없이 **UART 만으로 복구**.

```
[Stage 2 깨짐]
    ↓
Stage 1 의 verify FAIL → recovery slot 으로 fallback
    ↓
recovery main 이 active mode 시작
    ↓
host (PC) ←─── UART ───→ board (recovery)
    │                          │
    │ uart_uploader.py         │ recovery main
    │ + new Stage 2 binary     │ + flash erase/program
    ↓                          ↓
"DONE" 받으면 board reset
    ↓
정상 부팅 path 재개 (Stage 1 → 새 Stage 2 → slot)
```

### 보안 범위

- **무결성 (Integrity)**: ✅ CRC32 로 transmission 손상 검출
- **인증 (Authentication)**: ❌ — 누구든 UART 연결만 되면 새 Stage 2 program 가능
- **보안 (Security)**: ❌ — Secure Boot (HAB + 서명 + eFuse) 별도 마일스톤

---

## 2. Frame Format

5 phase sequence. 각 phase 에서 양방향 communication.

### Phase 1: Handshake

```
HOST  → BOARD:  "RECV?\n"     (6 bytes ASCII)
BOARD → HOST:   "READY!\n"    (7 bytes ASCII)
```

- HOST 가 board 의 활성 상태 확인
- BOARD 가 ready 응답 못 보내면 recovery 안 들어간 것 (Stage 2 정상 부팅 중)

### Phase 2: Size negotiation

```
HOST  → BOARD:  size            (4 bytes, little-endian uint32)
BOARD → HOST:   "ACK\n"  또는  "TOOBIG\n"
```

- size 한도: `MAX_STAGE2_SIZE = 0x3C000` (240 KB, Stage 2 영역)
- size > MAX: "TOOBIG\n" 보내고 IDLE 로 복귀
- size OK: "ACK\n" 보내고 RECV_DATA 단계로

### Phase 3: Data transfer

```
HOST  → BOARD:  data            (size bytes, raw binary)
```

- 받은 byte 를 OCRAM buffer 에 누적
- 별도 ACK 없음 (size bytes 다 받을 때까지 단순 누적)

### Phase 4: CRC verify

```
HOST  → BOARD:  crc32           (4 bytes, little-endian uint32)
BOARD → HOST:   "CRCOK\n"  또는  "CRCFAIL\n"
```

- CRC 범위: **DATA 만** (size 와 별도)
- BOARD 가 받은 data 의 CRC32 계산 → host 의 CRC 와 비교
- mismatch: "CRCFAIL\n" 보내고 IDLE 로 복귀 (host 재시도 가능)

### Phase 5: Flash + verify

```
BOARD:          erase Stage 2 region (0x60004000 ~ 0x60040000)
BOARD:          program OCRAM buffer to 0x60004000
BOARD:          re-read + CRC verify (post-program)
BOARD → HOST:   "DONE\n"  또는  "FLASHFAIL\n"
BOARD:          NVIC reset (정상 시)
```

- BootCtrl_LowLevel_EraseSector + BootCtrl_LowLevel_ProgramPage 활용 (Mission 2 의 ITCM lowlevel API)
- post-program CRC 검증으로 flash write 자체의 손상도 detect
- 정상 시 NVIC reset → Stage 1 재진입 → 새 Stage 2 의 verify 통과 → 정상 부팅

---

## 3. State Machine (Recovery Side)

```
┌─────────────────────────────────────────────────────┐
│                    IDLE                              │
│  (recovery main 진입 직후)                           │
│  UART RX 대기                                        │
└─────────────────────┬───────────────────────────────┘
                      │ "RECV?" 받음
                      ▼
┌─────────────────────────────────────────────────────┐
│                  HANDSHAKE                           │
│  "READY!\n" 보냄                                     │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│              RECV_SIZE                               │
│  4 bytes (LE uint32) 받음                            │
│  validate (size <= MAX_STAGE2_SIZE)                  │
│  ├─ TOOBIG: "TOOBIG\n" → IDLE                        │
│  └─ OK:     "ACK\n"     → RECV_DATA                  │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              RECV_DATA                               │
│  size bytes 받아 OCRAM buffer 에 누적                │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              RECV_CRC                                │
│  4 bytes (LE uint32) 받음                            │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              VERIFY_CRC                              │
│  CRC32_Compute(buffer, size) vs received_crc         │
│  ├─ mismatch: "CRCFAIL\n" → IDLE                     │
│  └─ match:    "CRCOK\n"   → FLASH                    │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              FLASH                                   │
│  1. erase Stage 2 region (0x60004000~0x60040000)     │
│     - sector by sector (4 KB each)                   │
│  2. program buffer to 0x60004000                     │
│     - page by page (256 bytes each)                  │
│  3. re-read CRC32 from 0x60004000                    │
│     - 받은 data 의 CRC 와 비교 (post-program 검증)   │
└─────────────────────┬───────────────────────────────┘
                      ▼
            ┌─────────┴─────────┐
            ▼                    ▼
       flash OK              flash FAIL
       "DONE\n"              "FLASHFAIL\n"
       NVIC reset            halt (LED 빠른 깜빡)
                             → 사용자 SW7 reset 으로 IDLE
```

---

## 4. Constants

```c
/* Magic strings */
#define RECOVERY_REQ        "RECV?\n"        /* HOST → BOARD: 시작 신호 */
#define RECOVERY_READY      "READY!\n"       /* BOARD → HOST: 응답 */
#define RECOVERY_ACK        "ACK\n"          /* BOARD → HOST: size OK */
#define RECOVERY_TOOBIG     "TOOBIG\n"       /* BOARD → HOST: size 한도 초과 */
#define RECOVERY_CRCOK      "CRCOK\n"        /* BOARD → HOST: CRC 통과 */
#define RECOVERY_CRCFAIL    "CRCFAIL\n"      /* BOARD → HOST: CRC 실패 */
#define RECOVERY_DONE       "DONE\n"         /* BOARD → HOST: 모든 단계 완료 */
#define RECOVERY_FLASHFAIL  "FLASHFAIL\n"    /* BOARD → HOST: flash write 실패 */

/* Limits */
#define MAX_STAGE2_SIZE     0x3C000u         /* 240 KB (Stage 2 영역 size) */

/* Memory */
#define STAGE2_BASE         0x60004000u
#define RECOVERY_BUFFER     0x20200000u      /* OCRAM 시작 */
```

---

## 5. Host 측 동작 (Python: tools/uart_uploader.py)

```python
import serial, struct, zlib, sys

def upload(port, baud, binary_path):
    with open(binary_path, 'rb') as f:
        data = f.read()
    size = len(data)
    crc = zlib.crc32(data) & 0xFFFFFFFF

    ser = serial.Serial(port, baud, timeout=10)

    # Phase 1
    ser.write(b"RECV?\n")
    if ser.readline().strip() != b"READY!":
        sys.exit("BOARD not ready")

    # Phase 2
    ser.write(struct.pack("<I", size))
    if ser.readline().strip() != b"ACK":
        sys.exit("size rejected")

    # Phase 3
    ser.write(data)

    # Phase 4
    ser.write(struct.pack("<I", crc))
    if ser.readline().strip() != b"CRCOK":
        sys.exit("CRC fail")

    # Phase 5
    if ser.readline().strip() != b"DONE":
        sys.exit("flash fail")

    print("OK — board will reset")
```

---

## 6. Error Handling

| 상황 | BOARD 응답 | 다음 동작 |
|---|---|---|
| size 한도 초과 | "TOOBIG\n" | IDLE 복귀 — host 재시도 가능 |
| CRC mismatch | "CRCFAIL\n" | IDLE 복귀 — host 재시도 가능 (transmission 오류) |
| flash erase 실패 | "FLASHFAIL\n" | halt + LED 빠른 깜빡 — SW7 reset 으로 복구 |
| flash program 실패 | "FLASHFAIL\n" | 동일 |
| post-program CRC 실패 | "FLASHFAIL\n" | 동일 (write 자체 손상) |
| host timeout | (응답 없음) | host 가 detect (10초) |

### Idempotency

- IDLE → HANDSHAKE → RECV_SIZE → RECV_DATA → RECV_CRC → VERIFY_CRC 의 모든 단계는 **stateless**
- IDLE 로 복귀하면 처음부터 다시 시도 가능
- FLASH 단계만 destructive — 시작되면 끝까지

---

## 7. Trade-off 와 미래 확장

### 현재 spec 의 단순화

- **No chunking**: data 를 한 번에 다 받음 (240 KB 까지)
  - XMODEM 처럼 1KB block + ACK/NAK 안 함
  - host 의 buffer overflow 위험은 OCRAM 256 KB 로 충분히 cover
- **No retry on transmission error**: CRC 실패 시 host 가 처음부터 다시
  - block 단위 재전송 안 함
- **No size encoding**: size 는 raw 4 bytes
  - varint 같은 압축 안 함

### 향후 확장 가능

- 1 KB chunk + per-chunk ACK (XMODEM 패턴)
- timeout 처리 (현재는 host 측만)
- multi-stage update (Stage 1 도 update?)
- Secure Boot 결합 (서명 검증 후 program)

---

## 8. 검증 시나리오 (Step 4-D)

### 정상 시나리오

```bash
# 1. Stage 2 의도적 손상
pyocd erase --target mimxrt1020 --sector 0x60004000

# 2. SW7 reset → recovery 진입 (passive 까지의 검증)
# 3. UART download
python3 tools/uart_uploader.py /dev/ttyACM0 115200 build/bootloader/stage2/bootloader_stage2.bin

# 4. board 자동 reset → 정상 부팅 path 재개
# 5. UART 의 정상 흐름 확인 (Stage 2 OK → ... → heartbeat)
```

### CRC mismatch 시나리오

```bash
# host 에서 CRC 일부러 잘못 계산해서 보냄 → "CRCFAIL\n" 받아야
python3 tools/uart_uploader.py --corrupt-crc /dev/ttyACM0 115200 build/...
```

### Size 한도 초과 시나리오

```bash
# host 에서 size > 240KB 보냄 → "TOOBIG\n" 받아야
python3 tools/uart_uploader.py --fake-size 0x3D000 /dev/ttyACM0 115200 build/...
```

---

## 9. 관련 자료

- `docs/PATH_B3_20260502_PHASE4_PASSIVE_RECOVERY.md` — Phase 4 Step 1-3 (passive)
- `bootloader/recovery/src/main.c` — recovery application (Step 4 에서 active 로 확장)
- `tools/uart_uploader.py` — host script (Step 4-C 에서 작성)
- `shared/include/crc32.h` — CRC32 함수
- `shared/src/bootctrl_rom_api.c` — flash erase/program (재사용)
