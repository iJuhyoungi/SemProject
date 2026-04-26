# Path B Step 1 (B-1) — Mission 3 닫기: Bootloader 의 boot_attempts 증가 + Rollback 검증

> 작업 일자: 2026-04-26
> 결과: **Mission 3 = 100% 완료**. OTA A/B 의 trial → fail → rollback → recovery 사이클 검증됨.

---

## 0. 출발점 (B-1 시작 시점 상태)

### 미션 3 의 결손

기존 `Boot_SelectSlot` 은 trial / rollback 분기 로직은 있었지만:

```c
if (Boot_IsPendingTrial(ctrl)) {
    if (ctrl->boot_success == BOOT_SUCCESS_NO &&
        ctrl->boot_attempts < BOOT_MAX_ATTEMPTS) {     /* MAX = 1 */
        return ctrl->pending_slot;
    }
    return Boot_GetFallbackSlot(ctrl);
}
```

**`boot_attempts` 를 증가시키는 코드가 어디에도 없음**. `bootloader/src/main.c` 에 의도는 주석으로만:

```c
// /* 추가 */
// if (Boot_IsPendingTrial(ctrl) && ctrl->pending_slot == slot) {
//     BootCtrl_MarkTrialAttempt(slot);
// }
```

→ 결과: trial slot 이 confirm 못 하면 매 boot 마다 attempts=0 → 다시 trial → 무한 루프.
**rollback 영원히 발동 안 함.**

### 추가로 살린 것

- 미션 2 의 일부 (lowlevel 함수의 `.ramfunc` 배치) 도 같이 시작
  - bootloader 가 자기 binary 안에 ROM API 호출 코드를 가져야 하는데,
    bootloader 는 FLASH XIP 실행 구조라 erase 중 자기 코드 fetch = 충돌
  - → ROM API 호출 코드는 ITCM 으로 옮겨야 함 = `.ramfunc` 도입

---

## 1. Path B 전체 목표 (다시)

| Step | 미션 | 결과 |
|---|---|---|
| **B-1** | Mission 3 닫기 (boot_attempts 증가 + rollback 검증) | ✅ 본 문서 범위 |
| B-2 | Mission 2 마무리 (slot A/B 의 lowlevel 도 `.ramfunc` 로) | 다음 |
| B-3 | 2-stage bootloader refactor | 그 다음 |

---

## 2. B-1 작업 흐름

### 2-1. Linker + Startup 인프라 추가

**대상 파일:**
- `bootloader/linker/bootloader.ld`
- `shared/src/startup_MIMXRT1020.c`

**bootloader.ld 변경:**

```
MEMORY
{
    ...
    ITCM         (rwx): ORIGIN = 0x00000000, LENGTH = 0x00010000   /* 추가 */
    DTCM         (rwx): ORIGIN = 0x20000000, LENGTH = 0x00010000
}

SECTIONS
{
    ...
    .text : { ... } > FLASH

    /* 추가: ROM API 호출 코드를 ITCM 에 두기 위한 섹션 */
    .ramfunc :
    {
        . = ALIGN(4);
        __ramfunc_start__ = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > ITCM AT> FLASH
    __ramfunc_load_start__ = LOADADDR(.ramfunc);
    ...
}
```

**startup_MIMXRT1020.c 의 `IS_BOOTLOADER` 분기에 ramfunc 복사 추가:**

```c
#if defined(IS_BOOTLOADER)
    UART1_Init();

    volatile uint32_t *src, *dst;

    src = (volatile uint32_t *)&__data_load_start__;
    dst = (volatile uint32_t *)&__data_start__;
    while (dst < (volatile uint32_t *)&__data_end__) { *dst++ = *src++; }

    /* 추가: .ramfunc 를 FLASH 에서 ITCM 으로 복사 */
    src = (volatile uint32_t *)&__ramfunc_load_start__;
    dst = (volatile uint32_t *)&__ramfunc_start__;
    while (dst < (volatile uint32_t *)&__ramfunc_end__) { *dst++ = *src++; }

    for (dst = (volatile uint32_t *)&__bss_start__; ...) { *dst++ = 0U; }
```

### 2-2. 파일 이름 refactor (기존 slot A/B 도 같이)

기존 컨벤션이 직관적이지 않다고 판단:

| 변경 전 | 변경 후 | 이유 |
|---|---|---|
| `bootctrl_runtime_flash.c` | `bootctrl_runtime_write.c` | "write 흐름" 이 본질, "flash" 는 매개체 |
| `bootctrl_runtime_flash_lowlevel.c` | `bootctrl_rom_api.c` | "ROM API" 임을 이름이 직접 알려줌 |

`git mv` 로 history 보존하면서 rename. slot A 와 slot B 양쪽 동시에. `bootctrl_runtime.h` / `bootctrl_runtime.c` (top-level dispatcher) 는 유지.

**Timing 의 의미:** bootloader 측 새 파일 추가 직전에 rename 함으로써 새 파일들은 처음부터 새 컨벤션으로 시작.

### 2-3. Bootloader 측 4개 신규 파일

| 파일 | 역할 |
|---|---|
| `bootloader/include/bootctrl_runtime_boot.h` | 인터페이스 (`BootCtrl_MarkTrialAttempt` 선언) |
| `bootloader/src/bootctrl_runtime_boot.c` | Top-level dispatcher (precheck → write 호출) |
| `bootloader/src/bootctrl_runtime_write_boot.c` | Write 흐름 (image build, shadow prepare, erase, program, verify) |
| `bootloader/src/bootctrl_rom_api.c` | ROM API wrapper. **모든 함수에 `BOOTCTRL_RAMFUNC` 어트리뷰트** |

**`bootctrl_rom_api.c` 핵심:**

```c
#define BOOTCTRL_RAMFUNC __attribute__((section(".ramfunc")))

BOOTCTRL_RAMFUNC
static void BootCtrl_MemCpy8(...) { ... }

BOOTCTRL_RAMFUNC
static int BootCtrl_RomInitIfNeeded(void) { ... }

BOOTCTRL_RAMFUNC
int BootCtrl_LowLevel_EraseSector(uint32_t address)
{
    /* 0x1771 fix 그대로 적용: AMBA-relative offset 변환 */
    uint32_t flash_offset = address - BOOTCTRL_FLEXSPI_AMBA_BASE;
    status_t st = api->flexSpiNorDriver->erase(0, &cfg, flash_offset, BOOTCTRL_SECTOR_SIZE);
    ...
}
```

**`bootctrl_runtime_write_boot.c` 핵심 — image build 가 slot A 와 다름:**

```c
/* slot A (참고): hardcoded SLOT_A confirm */
/* static int BootCtrl_RuntimeBuildConfirmImage_SlotA(boot_ctrl_t *out) { 
       out->active_slot = SLOT_A;
       out->pending_slot = SLOT_NONE;
       out->boot_success = BOOT_SUCCESS_YES;
       out->boot_attempts = 0;
   } */

/* bootloader: 현재 metadata 보존 + attempts 만 +1 */
static int BootCtrl_BuildAttemptIncrementImage(boot_ctrl_t *out, const boot_ctrl_t *current)
{
    if (!out || !current) return 0;
    *out = *current;            /* 현재 그대로 복사 */
    out->boot_attempts++;       /* attempts 만 증가 */
    return 1;
}
```

### 2-4. main.c 활성화

```c
#include "bootctrl_runtime_boot.h"   /* 추가 */

const boot_ctrl_t *ctrl = Boot_GetCtrl();
uint32_t slot = Boot_SelectSlot(ctrl);   /* 이 시점 attempts 로 결정 */
uint32_t base = Boot_GetSlotBase(slot);

if (Boot_IsPendingTrial(ctrl) && ctrl->pending_slot == slot) {
    UART1_SendString("[BOOT] Marking trial attempt...\r\n");
    if (BootCtrl_MarkTrialAttempt(ctrl)) {
        UART1_SendString("[BOOT] attempts incremented OK\r\n");
    } else {
        UART1_SendString("[BOOT] attempts increment FAILED\r\n");
    }
}
```

### 2-5. 검증 시나리오

**셋업:**
- `boot_ctrl_default.c` 를 trial-B 로: `pending=SLOT_B`, `success=NO`, `attempts=0`
- `app_slot_b/src/main.c` 의 `BootCtrl_RuntimeConfirm_SlotB()` 호출 블록 일시 주석 처리 (= confirm 일부러 안 함)

**1번째 boot 기대 출력:**
```
[BOOT] Marking trial attempt for selected slot...
[BOOT] attempts incremented OK
[BOOT] Pending trial detected
[BOOT] pending_slot = B
[BOOT] boot_attempts = 0x00000001     ← flash 갱신 후 read
[BOOT] Selected slot: B               ← 결정은 attempts=0 시점
[APP-B] main entered
[APP-B] bootctrl.boot_attempts = 0x00000001
[APP-B] confirm write SKIPPED
[APP-B] heartbeat ...
```

**2번째 boot 기대 출력:**
```
                                       ← Marking 메시지 없음 (slot=A != pending=B)
[BOOT] Pending trial detected
[BOOT] boot_attempts = 0x00000001
[BOOT] Selected slot: A               ← rollback 발동!
[APP-A-SDRAM] main entered
[APP-A-SDRAM] Phase B confirm write SUCCESS    ← 기존 C-2.3 confirm 으로 stable 정리
```

**실제 결과: 둘 다 정확히 매칭됨**. 미션 3 검증 완료.

---

## 3. 트러블슈팅 기록

### T-1: 빈 `bootctrl_writer.c` 잔재

**증상:** 빌드 로그에 `bootctrl_writer.c.obj` 가 빌드됨. 이름 회의에서 폐기한 컨벤션.

**원인:** 초기 가이드에서 `bootctrl_writer*.c` 이름을 제안했고, 그 시점에 빈 파일을 미리 만들어두었음. 이후 컨벤션이 `bootctrl_runtime_boot*` 로 바뀌면서 leftover.

**해결:** `git rm bootloader/src/bootctrl_writer.c`.

**교훈:** 신규 파일 이름은 컨벤션 확정된 후에만 만든다. 미리 만들면 leftover 처리 비용.

### T-2: `implicit declaration of function 'UART1_SendString'`

**증상:**
```
app_slot_a/src/bootctrl_runtime_write.c:212:5: warning: implicit declaration
```

**원인:** rename 전부터 있던 latent bug. `bootctrl_runtime_write.c` 가 `UART1_SendString` 호출하면서 `uart.h` include 안 함. GCC 의 implicit declaration tolerance 로 link 단계에서 해결되니 동작은 하지만 strict 모드에서 fail.

**해결:** `#include "uart.h"` 한 줄 추가.

**교훈:** rename 같은 mechanical refactor 가 도리어 묵은 warning 을 수면 위로 올린다. 좋은 부수 효과.

### T-3: `undefined reference to '__ramfunc_end__'`

**증상:** linker 에러
```
ld: ... undefined reference to `__ramfunc_end__'
```

**원인:** `bootloader.ld` 의 `.ramfunc` 섹션 정의에서:
```
__ramfunc_end = .;       ← 끝에 __ (double underscore) 빠짐
```
`startup_MIMXRT1020.c` 는 `__ramfunc_end__` (double underscore) 를 참조하는데 linker 에 정의된 건 `__ramfunc_end` (single).

**해결:** linker 의 심볼 이름 한 글자 수정 → `__ramfunc_end__`.

**교훈:** 베어메탈 startup ↔ linker script 사이의 심볼 이름은 toolchain 컨벤션이 미묘하다 (보통 trailing `__` 두 개). slot A 의 패턴을 그대로 복붙하는 게 안전.

### T-4: `.ramfunc` 섹션 size = 0x0 (linker GC 로 다 날아감)

**증상:** map 파일에서:
```
.ramfunc        0x00000000        0x0 load address 0x60002b30
```
어트리뷰트는 다 붙였는데 size 가 0.

**진단:**
- obj 파일 내부에는 `.ramfunc` size = 0x274 (628 바이트) 로 정상 존재
- 그러나 linker 가 최종 ELF 에서 통째로 discard

**원인:** `bootloader/CMakeLists.txt` 가 `-Wl,--gc-sections` 사용. **호출되지 않는 함수/섹션을 dead code 로 보고 제거**. 당시 main.c 가 `BootCtrl_MarkTrialAttempt` 를 호출하지 않거나 호출 chain 이 형성 안 된 상태였음.

**해결:** 다음 step (B-1.2 ~ B-1.4) 에서 호출 체인이 자연스럽게 형성됨 → `.ramfunc` 가 0x274 로 채워짐.

**교훈:** `--gc-sections` 환경에서 "심볼 정의" 만으로는 binary 에 안 들어감. **reachable** 해야 함. 디버깅 시 단순히 어트리뷰트만 확인하지 말고 호출 체인이 main 까지 연결되어 있는지 확인 필요.

### T-5: `boot_ctrl_default.c` 가 trial-B 시나리오가 아니었음

**증상:** rollback 검증 시리얼에서:
```
[BOOT] Selected slot: A           ← B 가 아니라 A
[APP-A-SDRAM] main entered        ← slot A 가 부팅됨
```

기대했던 `[BOOT] Marking trial attempt...` 가 안 보임.

**진단:**
```
$ cat bootctrl/src/boot_ctrl_default.c
.pending_slot = SLOT_NONE,        ← B 가 아님!
```

**원인:** 이전 세션에서 trial-B 로 바꿨다가 어디선가 다시 `SLOT_NONE` 으로 reverted. flash 에는 default 값이 그대로 굽혀있으니 trial 이 아예 시작 안 함.

**해결:** `pending_slot = SLOT_B` 로 다시 변경 + `flash_mcu.sh` 전체 굽기.

**교훈:** "**검증 시나리오의 입력 조건**" 이 맞는지 매 단계 확인. 코드만 옳다고 동작 안 함. 데이터 (= flash 의 metadata 초기값) 도 시나리오에 맞아야.

### T-6: 시리얼 캡처 타이밍 (1번째 boot 못 잡음)

**증상:** rollback 검증 시 캡처 화면에 1번째 boot 의 출력이 안 보임. attempts 가 처음부터 1로 보여서 "기대와 다르다" 고 오해.

**원인:** 캡처 시작 → reset → boot 진행 의 순서를 정확히 동기화 안 했음. 캡처가 너무 늦게 시작되거나, reset 을 누른 후 다시 시작했거나.

**해결:**
1. `timeout 30 cat /dev/ttyACM0 > /tmp/log &` 로 백그라운드 캡처 먼저 시작
2. 즉시 `./flash_mcu.sh metadata` 로 attempts 를 0 으로 리셋
3. SW7 reset → 1번째 boot
4. 30초 안에 또 reset → 2번째 boot
5. 캡처 종료 후 분석

**교훈:** 시리얼 디버깅의 캡처 타이밍은 시나리오 검증의 일부. "기대와 다르다" 의 절반은 코드가 아니라 캡처 타이밍.

### T-7: `boot_attempts` print 값이 0 이 아닌 1

**증상:** 1번째 boot 의 시리얼:
```
[BOOT] Marking trial attempt...
[BOOT] attempts incremented OK
[BOOT] boot_attempts = 0x00000001    ← ★ 왜 1? 0이어야 하는 것 아닌가?
[BOOT] Selected slot: B
```

**원인 (실은 정상):**
1. `slot = Boot_SelectSlot(ctrl)` 에서 attempts=0 보고 SLOT_B 결정 (= `slot` 변수에 박힘)
2. `BootCtrl_MarkTrialAttempt(ctrl)` 가 flash 에 attempts=1 작성
3. print 단계에서 `ctrl->boot_attempts` 다시 read (`ctrl` 은 flash 포인터) → flash 가 갱신됐으니 1
4. `Selected slot: B` 는 1번에서 박은 변수 출력

즉 **"결정 시점" 의 값과 "출력 시점" 의 값이 다른 게 정상**.

**교훈:** 같은 포인터를 dereference 해도 underlying memory 가 갱신되면 다른 값이 나온다. 베어메탈에선 `volatile` 한 메모리(flash, register, DMA buffer)가 그렇게 동작하는 게 일반적. 이 "시간 의미" 를 명확히 추적하는 습관이 디버깅에 결정적.

---

## 4. 핵심 인사이트

### I-1: `--gc-sections` 와 dead-code elimination

함수에 `__attribute__((section(".ramfunc")))` 만 붙인다고 binary 에 들어가는 게 아니다. **호출 체인이 entry point (Reset_Handler) 까지 reachable** 해야 한다.

검증법: `grep "^\.ramfunc" build/bootloader/bootloader.map` 의 size 값이 obj 의 .ramfunc size 와 같은지 확인. 다르면 GC 됨.

### I-2: AHB 주소 vs Flash device offset (재확인)

slot A 의 0x1771 fix 와 동일 패턴이 bootloader 에도 그대로 적용. ROM API 는 flash-base offset (`0x3F0000`), XIP read 는 AMBA 절대주소 (`0x603F0000`). 같은 sector 의 두 좌표계.

### I-3: `.ramfunc` 와 ITCM 의 의미

bootloader 는 FLASH XIP 실행 구조라서, flash erase 진행 중 자기 코드를 fetch 하면 충돌. 그래서 ROM API 호출 경로의 함수들 (lowlevel + helper) 는 ITCM 으로 옮겨 실행해야 안전.

slot A/B 는 `.text → SDRAM` 재배치 구조라 lowlevel 이 SDRAM 에 있어도 우연히 안전 (FlexSPI 와 별 bus). 그러나 미션 의 의도 ("ITCM 에 복사하여 실행") 는 정확히 만족 안 됨 — B-2 에서 마무리 예정.

### I-4: flash write 의 timing — "결정 시점" vs "갱신 후 read 시점"

main.c 코드 흐름의 미묘한 부분:

```c
const boot_ctrl_t *ctrl = Boot_GetCtrl();              // 포인터
uint32_t slot = Boot_SelectSlot(ctrl);                 // 결정 시점 (read 1)
                                                        // → slot 변수에 박힘

BootCtrl_MarkTrialAttempt(ctrl);                       // flash 갱신 (write)

UART1_SendHex32(ctrl->boot_attempts);                  // 갱신 후 read (read 2)
                                                        // → 새 값 나옴
UART1_SendString((slot==B) ? "B" : "A");               // 결정 시점 값 그대로
```

같은 `ctrl` 포인터인데, MarkTrialAttempt 전후로 값이 다르다. 디버깅 시 "왜 selected 와 attempts print 가 안 맞는 것 같지?" 라는 의문이 생기면 이 구조를 떠올릴 것.

**일반화:** flash / register / DMA buffer 같이 underlying memory 가 갱신될 수 있는 자원은, 같은 포인터 dereference 도 시점에 따라 다른 값을 줄 수 있다. → 이 인사이트는 SSD FW 의 NAND status register polling, FTL 의 mapping table 갱신 등에서 동일하게 등장.

### I-5: 트러블슈팅 시간 분포 — 코드보다 셋업

12일 묵은 0x1771 디버깅에 비하면 B-1 은 매우 빨리 풀렸지만, 시간 분포를 회고하면:

| 단계 | 시간 비중 |
|---|---|
| 코드 작성 (4개 파일) | ~30% |
| Linker / startup 인프라 | ~15% |
| 컨벤션 정리 (rename refactor) | ~10% |
| **검증 셋업 / 캡처 타이밍 / 시나리오 입력 (T-5, T-6)** | **~30%** |
| 결과 해석 (T-7) | ~15% |

**약 절반이 "코드" 가 아닌 "셋업과 해석"**. 이게 베어메탈/펌웨어 디버깅의 일반 분포라고 봐야 한다. 실제 산업계 SSD/메모리 펌웨어 검증도 같은 구조 — test vector / test setup / log 분석이 디버깅 시간의 상당 부분.

### I-6: rename refactor 의 timing

기존 코드의 이름이 어색하다고 느낄 때:
- ❌ "나중에 정리하지" → 새 파일이 같은 컨벤션으로 늘어나면서 rename 비용이 곱해짐
- ✅ **새 파일 추가 직전**에 rename → slot A + slot B 만 건드림. bootloader 새 파일은 새 컨벤션으로 시작

이번에 정확히 이 pattern. `bootctrl_runtime_flash.c → bootctrl_runtime_write.c`, `bootctrl_runtime_flash_lowlevel.c → bootctrl_rom_api.c` 를 bootloader 새 파일 만들기 직전에 처리.

---

## 5. 검증된 OTA cycle (전체 그림)

```
┌─ Initial state (./flash_mcu.sh 직후) ────────────────┐
│ flash @ 0x603F0000:                                   │
│   active=A, pending=B, success=NO, attempts=0         │
└───────────────────────────────────────────────────────┘
              ↓ SW7 reset (1차)
┌─ 1st boot ───────────────────────────────────────────┐
│ Bootloader:                                           │
│   Boot_SelectSlot: attempts=0 < MAX(1) → SLOT_B      │
│   Boot_IsPendingTrial && pending==slot → MarkAttempt  │
│   MarkTrialAttempt:                                   │
│     ROM API erase(0x603F0000-0x60000000)             │
│     ROM API program with attempts++=1                 │
│   flash 갱신: attempts=1                              │
│   → Slot B 점프 (0x60200000)                          │
│                                                       │
│ Slot B:                                               │
│   bootctrl read → attempts=1 (갱신된 값)              │
│   confirm write 호출 — 검증 위해 일시 비활성화        │
│   → confirm 안 함. heartbeat 무한                     │
└───────────────────────────────────────────────────────┘
              ↓ SW7 reset (2차)
┌─ 2nd boot ───────────────────────────────────────────┐
│ Bootloader:                                           │
│   Boot_SelectSlot: attempts=1 ≥ MAX(1) → fallback    │
│                                       = active = A    │
│   pending==B != slot==A → MarkAttempt 안 함          │
│   → Slot A 점프 (0x60040000)              ★ rollback! │
│                                                       │
│ Slot A:                                               │
│   기존 C-2.3 confirm 흐름 동작                        │
│   → flash 갱신: active=A, pending=NONE,              │
│                 success=YES, attempts=0   ← stable   │
└───────────────────────────────────────────────────────┘
```

이 두 boot 사이클이 OTA A/B 의 **trial → fail → rollback → recovery** 의 완전한 회로.

---

## 6. 변경 파일 요약

### 신규
- `bootloader/include/bootctrl_runtime_boot.h`
- `bootloader/src/bootctrl_runtime_boot.c`
- `bootloader/src/bootctrl_runtime_write_boot.c`
- `bootloader/src/bootctrl_rom_api.c`

### 수정
- `bootloader/linker/bootloader.ld` (ITCM region + .ramfunc 섹션)
- `shared/src/startup_MIMXRT1020.c` (IS_BOOTLOADER 분기에 .ramfunc copy)
- `bootloader/src/main.c` (`BootCtrl_MarkTrialAttempt(ctrl)` 호출 활성화 + 헤더 include)
- `bootctrl/src/boot_ctrl_default.c` (검증 위해 trial-B 로 변경, 검증 후 원복)
- `app_slot_b/src/main.c` (검증 위해 confirm 호출 일시 주석, 검증 후 원복)

### Rename (B-1 시작 시 mechanical refactor)
- `app_slot_a/src/bootctrl_runtime_flash.c` → `bootctrl_runtime_write.c`
- `app_slot_a/src/bootctrl_runtime_flash_lowlevel.c` → `bootctrl_rom_api.c`
- `app_slot_b/src/bootctrl_runtime_flash_b.c` → `bootctrl_runtime_write_b.c`
- `app_slot_b/src/bootctrl_runtime_flash_lowlevel.c` → `bootctrl_rom_api.c`

---

## 7. 다음 단계

### B-2 — Mission 2 100% 마무리

slot A 와 slot B 의 lowlevel 함수들도 `.ramfunc` 어트리뷰트 부여 + ITCM 으로 이동. 옵션으로 `bootctrl_rom_api.c` 의 3개 카피 (slot A / slot B / bootloader) 를 `shared/` 로 통합.

작업 요지:
- `app_slot_a/src/bootctrl_rom_api.c` 의 함수들에 `BOOTCTRL_RAMFUNC` 추가
- 동일하게 `app_slot_b/src/bootctrl_rom_api.c` 도
- 검증: map 파일에서 lowlevel 함수가 ITCM (0x000xxxxx) 에 위치하는지 확인
- 추가로 fault handler 도 `.ramfunc` 검토

### B-3 — 2-stage bootloader

bootloader 전체를 Stage1 (immutable, ~16-32KB) + Stage2 (A/B updatable) 로 분리. SSD 펌웨어의 BL1/BL2 패턴과 동일.

---

## 부록 — B-1 검증 단계 캡처 (재현용)

### 캡처 명령

```bash
# 1. 백그라운드 캡처 시작
timeout 30 cat /dev/ttyACM0 > /tmp/path_b1.log &
sleep 0.5

# 2. metadata 만 다시 굽기 (attempts=0 reset)
./flash_mcu.sh metadata

# 3. SW7 reset 1번째 → 1st boot
# 4. 15초 정도 기다림
# 5. SW7 reset 2번째 → 2nd boot
# 6. 캡처 종료까지 대기

# 7. 결과 보기
cat /tmp/path_b1.log
```

### 검증 셋업 원복 후 정상 운영 흐름 복귀

```bash
# bootctrl/src/boot_ctrl_default.c 원복
#   .pending_slot = SLOT_NONE
#   .boot_success = BOOT_SUCCESS_YES (또는 본인이 정한 default)

# app_slot_b/src/main.c 의 confirm 호출 주석 해제

./build.sh
./flash_mcu.sh
```

---

본 작업의 git history:
- B-1 작업의 commit 들은 `dev` 브랜치에 누적되어 있음 (현재 시점에 push 여부는 별도 확인)
- 관련 사전 작업: `355e4d9 fix(ota): use AMBA-relative offset for ROM flash erase/program` (slot A 의 0x1771 fix, B-1 의 `bootctrl_rom_api.c` 가 그대로 활용)
