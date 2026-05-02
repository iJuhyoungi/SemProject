# B-3 Phase 4: Passive Recovery 작업 기록 (2026-05-02)

> Phase 4 의 Step 1-3 — recovery slot 인프라 + Stage 2 의 CRC32 image header + Stage 1 의 검증/점프 로직.
> Active recovery (UART download via Step 4) 는 다음 세션.

---

## 1. 배경

### B-3 의 진행 상황 (Phase 4 직전)

이전 세션 (B-3 Phase 1-3) 에서 2-stage bootloader 의 골격 완성:

```
0x60000000 ~ 0x60004000  Stage 1 (immutable, 8 KB)
0x60004000 ~ 0x60040000  Stage 2 (updateable, 240 KB)
```

단 Stage 1 의 검증이 **vector sanity (SP/PC/Thumb bit) 만**.
검증 fail 시 **`for(;;) { wfi; }` 무한 halt** — recovery 경로 없음.

### Phase 4 의 의미

> "현관문은 만들었지만 안방 문 망가졌을 때 거기서 뭘 할지 안 정함"

Phase 4 의 목표 = "현관에 안방 문 수리 도구 비치":
- Stage 2 invalid 시 recovery slot 으로 fallback
- Recovery slot 안에 minimal application (passive 또는 active)
- "OTA buggy update 로 brick 되어도 복구 가능" narrative 완성

이번 세션 (Step 1-3) 은 **passive recovery** 까지. Step 4 (active recovery via UART download) 는 다음 세션.

---

## 2. 설계 결정

### Recovery 정책 — 별도 recovery slot

| 옵션 | 선택 |
|---|---|
| A. 별도 recovery slot + minimal main | ✅ |
| B. Stage 2 우회해서 slot A/B 직접 점프 | ❌ (책임 분리 어색) |
| C. UART download mode | 향후 Step 4 에서 확장 |
| D. 단순 boot_attempts++ + halt | ❌ (진짜 recovery 아님) |

A 의 인프라가 C 의 인프라와 동일 — passive 부터 시작 후 active 로 확장.

### Stage 2 검증 강도 — CRC32

| 검증 단계 | 비용 | 검출 가능 |
|---|---|---|
| vector sanity | 0 | 진짜 깨진 vector 만 |
| magic byte | 매우 낮음 | 부분 손상 |
| **CRC32** ← 선택 | 중간 | 모든 손상 (무결성) |
| SHA256 | 중간 | 강한 무결성 |
| SHA256 + 서명 | 높음 | 인증 (Secure Boot 시점) |

CRC32 가 학습 단계에 적합 + 산업 표준 (Secure Boot 전 단계).

> SHA256 + 서명은 Secure Boot 마일스톤에서 RT1020 의 HAB 와 결합해야 진짜 가치.
> CRC32 는 무결성 (corruption 검출) 만 담당.

### Recovery slot 위치 — 0x603E0000, 64 KB

```
0x60040000  app_slot_a 시작 (변경 X)
0x603E0000  Recovery slot ★ NEW
0x603F0000  boot_ctrl metadata (변경 X)
```

slot A/B 영역과 boot_ctrl 사이의 빈 공간 활용 → 다른 영역 영향 X.

### 디렉토리 구조 — `bootloader/recovery/` (nested)

기존 `bootloader/{stage1,stage2}` 패턴에 일관:

```
bootloader/
├── stage1/
├── stage2/
└── recovery/   ★ NEW
```

Family resemblance — bootloader 의 internal architecture 의 일부.

### 메모리 맵 (Phase 4 후 최종)

```
0x60000000  ~ 0x60001000  FCB
0x60001000  ~ 0x60002000  IVT
0x60002000  ~ 0x60004000  STAGE 1 (immutable, 8 KB)
0x60004000  ~ 0x60040000  STAGE 2 (updateable, 240 KB)
0x60040000  ~ 0x60200000  app_slot_a (1.75 MB)
0x60200000  ~ 0x603E0000  app_slot_b (1.875 MB)   ← end shifted
0x603E0000  ~ 0x603F0000  RECOVERY (64 KB)         ★ NEW
0x603F0000  ~ 0x60400000  boot_ctrl metadata
```

---

## 3. 작업 단계 (Step 1-3)

### Step 1 — Recovery 인프라 (passive)

**파일 추가**:
- `bootloader/recovery/src/main.c` — passive recovery main (UART 알림 + LED 깜빡 + halt)
- `bootloader/recovery/linker/recovery.ld` — FLASH @ 0x603E0000, 64 KB
- `bootloader/recovery/CMakeLists.txt` — target = `bootloader_recovery`

**파일 수정**:
- `bootloader/CMakeLists.txt` — `add_subdirectory(recovery)` 추가

**Recovery main 동작**:
```c
int main(void) {
    UART1_SendString("[RECOVERY] Stage 2 invalid — recovery active\r\n");
    UART1_SendString("[RECOVERY] Please reflash via flash_mcu.sh\r\n");
    LED_Init();
    while (1) {
        LED_On();  delay_loop(5000000);
        LED_Off(); delay_loop(5000000);
    }
}
```

**빌드 결과**:
- `bootloader_recovery.bin` = 2.2 KB (64 KB 영역의 3% 사용)
- `.ramfunc` size = 0x0 (LowLevel API 안 호출 → `--gc-sections` 로 dead code 제거)

> Step 4 (active recovery) 추가 시 size 약 12 KB 예상 (UART protocol + flash write 로직).
> 64 KB 의 여유 충분.

### Step 2 — Stage 2 의 CRC32 image header

**핵심 — vector table 의 reserved entries 활용**:

```
vector[0] = __StackTop
vector[1] = Reset_Handler
vector[2..6] = exception handlers
vector[7] = ★ MAGIC (0xDEADBEEF)         ← post-build patch
vector[8] = ★ binary size                 ← post-build patch
vector[9] = ★ CRC32                       ← post-build patch
vector[10] = reserved (0)
vector[11..] = SVCall, ...
```

ARM Cortex-M 의 vector[7..10] 이 표준 reserved → image header 영역 활용. NXP SDK 일부에서 사용하는 패턴.

**파일 추가**:
- `tools/post_build_stage2.py` — Stage 2 의 .bin 후처리 (Python)

**파일 수정**:
- `bootloader/stage2/CMakeLists.txt` — POST_BUILD 에 script 호출 추가

**Post-build 동작**:
1. binary 의 size 계산
2. offset 0x1C (vector[7]) 에 magic 0xDEADBEEF write
3. offset 0x20 (vector[8]) 에 size write
4. offset 0x24 (vector[9]) 의 자리는 0 으로 두고 CRC32 계산 (self-reference 처리)
5. offset 0x24 에 계산된 CRC32 write

**검증**:
```python
# 자가검증
expected = struct.unpack_from('<I', data, 0x24)[0]
struct.pack_into('<I', data, 0x24, 0)
calculated = zlib.crc32(data) & 0xFFFFFFFF
assert expected == calculated
```

**Vector table 의 source-binary mismatch (의도된)**:
- C 코드 (`shared/src/startup_MIMXRT1020.c`): vector[7..10] = `{ .value = 0 }`
- Binary: post-build 가 patch 한 값
- 다른 binary (Stage 1 / recovery / slot) 는 patch 안 함 → vector[7..10] = 0 그대로 (dead data 없음)

### Step 3 — Stage 1 의 verify + recovery 점프

**파일 추가**:
- `shared/include/crc32.h`
- `shared/src/crc32.c` — IEEE 802.3 CRC-32 (zlib.crc32 호환), bit-by-bit 알고리즘

```c
uint32_t CRC32_Compute(const uint8_t *data, uint32_t len);

// CRC field 자체를 0 으로 가정하고 계산 (self-reference 처리)
uint32_t CRC32_ComputeWithSkip(const uint8_t *data, uint32_t len, uint32_t skip_offset);
```

**파일 수정**:
- `bootloader/stage1/src/main.c`
  - `Stage2_Verify()` 추가 — 4 단계 검증 (vector sanity → magic → size → CRC32)
  - main() 흐름 변경 — 검증 fail 시 recovery slot (0x603E0000) 으로 점프
  - `Jump_To_Stage2()` 함수 재사용 (generic image 점프)
- `flash_mcu.sh`
  - `flash_recovery()` 함수 추가
  - `case all` 에 포함, `recovery` 옵션 추가

**Verify 흐름**:
```c
static int Stage2_Verify(uint32_t base) {
    if (!Stage2_VectorSane(base))     return 0;   // 1. vector
    if (magic != 0xDEADBEEF)          return 0;   // 2. magic
    if (size 한도 밖)                  return 0;   // 3. size
    if (computed_crc != stored_crc)   return 0;   // 4. CRC32
    return 1;
}
```

---

## 4. 검증 시나리오 (3 가지)

### 4.1 정상 시나리오

```bash
./flash_mcu.sh   # 6 binary 굽기 (stage1, stage2, recovery, metadata, slot_a, slot_b)
# SW7 reset
```

UART:
```
[BL1] Stage 1 immutable
[BL1] Verifying Stage 2 ...
[BL1] Stage 2 OK - jumping to 0x60004000
 OTA Bootloader C-1 Started
[BOOT] Selected slot: A → Jumping to app...
[APP-A-SDRAM] main entered → ... → Phase B confirm SUCCESS → heartbeat
```

✅ 4단 boot chain 정상 동작.

### 4.2 비정상 시나리오 1 — Stage 2 sector erase

```bash
pyocd erase --target mimxrt1020 --sector 0x60004000
# SW7 reset
```

UART:
```
[BL1] Stage 1 immutable
[BL1] Verifying Stage 2 ...
[Verify] vector sanity FAIL                  ← 0x60004000 ~ 0x60004FFF 가 0xFF
[BL1] Stage 2 INVALID - jumping to recovery (0x603E0000)
[RECOVERY] Stage 2 invalid — recovery active
[RECOVERY] Please reflash via flash_mcu.sh
(LED 깜빡)
```

✅ Stage 1 → recovery 점프, recovery main 진입 + LED 깜빡 모두 정상.

### 4.3 비정상 시나리오 2 — CRC 만 corrupted (CRC 의 진짜 가치 검증)

```bash
cp build/bootloader/stage2/bootloader_stage2.bin /tmp/stage2_corrupt.bin
python3 -c "
with open('/tmp/stage2_corrupt.bin', 'r+b') as f:
    f.seek(0x24)
    f.write(b'\\x00\\x00\\x00\\x00')   # CRC field 만 0 으로
"
pyocd flash /tmp/stage2_corrupt.bin --target mimxrt1020 --base-address 0x60004000
# SW7 reset
```

UART:
```
[BL1] Stage 1 immutable
[BL1] Verifying Stage 2 ...
[Verify] CRC FAIL                            ← vector/magic/size 다 통과, CRC 만 fail
[BL1] Stage 2 INVALID - jumping to recovery (0x603E0000)
[RECOVERY] Stage 2 invalid — recovery active
[RECOVERY] Please reflash via flash_mcu.sh
```

✅ **CRC 의 진짜 가치 입증** — 표면 검증 (vector + magic + size) 다 통과해도 CRC 가 한 byte 손상까지 잡아냄.

### 두 비정상 시나리오 비교

| 시나리오 | 손상 | fail 단계 | 의미 |
|---|---|---|---|
| 1. Sector erase | Stage 2 통째 0xFF | vector sanity FAIL | 심한 손상 (큰 영역 깨짐) |
| 2. CRC 만 변경 | 한 word 만 0 | vector → magic → size → **CRC FAIL** | 미세 손상 (한 byte) |

**같은 결과 (recovery 발동) 지만 다른 단계에서 fail** → Stage 1 의 검증이 정확히 layered 동작.

---

## 5. 트러블슈팅

### 5.1 빌드 에러: `cannot find .=ALIGN`

**원인**: linker script 의 `. = ALIGN(4)` 문법에서 공백 누락 → linker 가 `.=ALIGN` 을 input file 로 잘못 해석.

```
변경 전:  .=ALIGN(4);
변경 후:  . = ALIGN(4);
```

**학습**: `. = ALIGN()` 은 location counter 의 assignment. linker 의 lexer 가 token 분리에 엄격.

### 5.2 빌드 경고: `'Stage2_Verify' defined but not used`

**원인**: main() 의 if 조건이 옛 함수 (`Stage2_VectorSane`) 그대로. `Stage2_Verify` 호출 안 함.

**Chain effect**:
1. `Stage2_Verify` unused → `--gc-sections` 가 dead code 로 제거
2. 그 안에서 호출하던 `CRC32_ComputeWithSkip` 도 reachable X
3. CRC32 함수 전체 GC → nm 에 안 보임

**Fix**: main() 의 if 조건 `Stage2_VectorSane` → `Stage2_Verify` 로 변경.

**학습**: GCC 의 unused warning 은 단순 cleanup 이슈가 아니라 **dead code chain 추적의 단서**.

### 5.3 비정상 시나리오 1차: recovery 메시지 안 뜸 + lockup

**증상**:
```
[BL1] Stage 2 INVALID - jumping to recovery (0x603E0000)
(여기서 멈춤, recovery 메시지 안 뜸)
```

SWD dump:
```
0x603E0000:  ff ff ff ff ff ff ff ff   ← recovery binary 굽혀있지 않음
"Device entered lockup"                  ← invalid jump (SP/PC = 0xFFFFFFFF)
```

**원인**: `flash_mcu.sh` 에 `flash_recovery` 처리가 안 들어감. recovery 영역 erased 상태 → Stage 1 이 jump 시도 시 invalid SP/PC → CPU lockup.

**Fix**: `flash_mcu.sh` 에 함수 추가:
```bash
flash_recovery() {
    pyocd flash build/bootloader/recovery/bootloader_recovery.bin $DEV --base-address 0x603E0000
}
```

`case all` 에 포함, `recovery` 옵션 추가.

**학습**: 새 binary 추가 시 빌드만 검증 X — flash 스크립트와 보드 검증까지 포함해서 end-to-end 확인.

### 5.4 빌드 에러: `does not contain a CMakeLists.txt`

**원인**: `bootloader/recovery/CMakeLists.txt` 누락. 디렉토리 구조 (src/, linker/) 만 만들고 CMakeLists 빠뜨림.

**Fix**: CMakeLists.txt 생성.

---

## 6. 인사이트 / 학습 포인트

### 6.1 Vector table 의 reserved entries 활용 (image header 패턴)

ARM Cortex-M 의 vector[7..10] 이 reserved (NXP SDK 일부에서도 image header 로 활용).

이 자리를 **post-build script 가 byte-level patch** — C 코드 안 건드림. NOR Flash 의 binary 만 patch 됨.

장점:
- C 소스의 vector table 정의는 그대로 (`{ .value = 0 }`)
- Post-build hook 으로 magic + size + CRC 한꺼번에 처리
- Stage 1 / recovery / slot 등 다른 binary 는 patch 안 함 → 영향 없음

### 6.2 CRC32 의 self-reference 처리

CRC field 자체가 binary 의 일부 → Chicken-and-egg.

해결: **CRC 자리를 0 으로 가정하고 계산** (post_build + runtime 둘 다 동일 규칙).

```c
uint32_t CRC32_ComputeWithSkip(const uint8_t *data, uint32_t len, uint32_t skip_offset)
{
    for (i = 0; i < len; i++) {
        byte = (i in [skip_offset, skip_offset+4)) ? 0 : data[i];
        crc = update(crc, byte);
    }
    return ~crc;
}
```

### 6.3 Layered 검증의 가치

```
Stage 1 의 verify:
    1. vector sanity (SP/PC/Thumb bit)
    2. magic (0xDEADBEEF)
    3. size 한도
    4. CRC32 (binary 전체)
```

각 단계가 다른 손상 패턴 검출:
- **1 단계 만**: 큰 영역 erased / 부팅 첫 진입 불가능
- **3 단계 까지 통과 + 4 단계 fail**: 미세 손상 (한 byte) — CRC 의 진짜 가치

면접 narrative 에서 "왜 vector sanity 만 아닌 CRC 까지?" 질문에 답:
- "binary 의 .text 또는 .data 영역의 부분 손상은 vector sanity 가 못 잡음"
- "CRC 가 binary 전체 무결성 검증"

### 6.4 GCC `--gc-sections` 의 chain effect

unused function 의 chain:
1. `Stage2_Verify` unused
2. → `--gc-sections` 가 제거
3. → 그 안에서 호출하던 `CRC32_ComputeWithSkip` 도 reachable X
4. → 함께 GC → nm 에 안 보임

**진단 단서**: GCC 의 `unused-function` warning. 빌드 통과해도 warning 무시 X.

### 6.5 Passive vs Active recovery 의 책임 분리

| | 인프라 | Application 책임 |
|---|---|---|
| Passive (지금) | recovery slot 메모리 + linker + Stage 1 의 검증/점프 | UART 알림 + LED + halt |
| Active (Step 4) | 동일 | UART download protocol + flash erase/program |

**A 의 인프라 위에 C 의 application 추가** — 인프라 재사용.

### 6.6 Image header 의 source-binary mismatch (의도된)

C 소스의 vector[7..10] = 0 그대로, binary 의 vector[7..10] 은 post-build patched.

**Cross-reference**: `shared/src/startup_MIMXRT1020.c` 의 vector table 정의에 주석 추가 권장 — 다음 사람이 헷갈리지 않도록 ("Stage 2 binary 만 patch" 명시).

---

## 7. 다음 단계 — Step 4 (Active Recovery)

Recovery 의 main 을 **passive → active** 로 확장:

```c
// 현재 (passive):
int main(void) {
    UART1_SendString("[RECOVERY] active\r\n");
    while(1) { LED blink; }
}

// 향후 (active, Step 4):
int main(void) {
    UART1_SendString("[RECOVERY] Waiting for new Stage 2 via UART...\r\n");
    receive_via_uart_protocol();   // XMODEM 또는 자체
    erase_stage2_region();         // BootCtrl_LowLevel_Erase
    program_stage2(received_data); // BootCtrl_LowLevel_Program
    if (verify_crc()) {
        reset_system();            // 정상 부팅 path 재개
    }
}
```

추가 작업 예상:
- UART protocol (XMODEM 또는 자체 단순 protocol)
- BootCtrl_LowLevel_* 함수 활용 (현재 Stage 2 / slot 만 사용 — recovery 도 link 하면 됨)
- 검증 후 reset

작업 비용: 4-6시간 (1 세션).

---

## 8. Commits

```
6044749 feat(boot): stage1 verifies stage2 CRC32 and falls back to recovery
3391620 feat(boot): add recovery slot infrastructure with stage2 CRC32
```

이전 B-3 commits (Phase 1-3) 는 별도 — `git log` 참조.

---

## 9. 면접 narrative 한 줄

> "OTA 의 trust chain 을 한 단계 더: Stage 2 의 buggy update 로 boot chain 자체가 깨져도 Stage 1 의 CRC32 검증이 fail 을 detect → recovery slot 으로 fallback. 안드로이드의 system A/B + bootloader stage 분리 + image integrity 의 결합."

> "검증은 layered: vector sanity → magic → size → CRC32. 같은 fail 결과지만 어느 단계에서 fail 됐는지로 손상의 패턴 파악 가능."

---

> 다음 세션 (B-3 Phase 4 Step 4) 에서 active recovery 구현 후 별도 docs 추가 예정.
