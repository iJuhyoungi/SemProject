# 🐛 Phase B Runtime Metadata Flash Write 디버깅 (2026-04-25)

## 📌 한 줄 결론

`bootctrl_runtime_flash_lowlevel.c` 에서 ROM API `flexspi_nor_flash_erase()` /
`flexspi_nor_flash_page_program()` 호출 시 **절대주소 `0x603F0000`** 을 그대로 넘기고
있었던 것이 root cause. RT1020 ROM API는 **flash base 로부터의 byte offset**(=
`0x003F0000`)을 받기 때문에 ROM이 입력 검증 단계에서 거절하면서 `status = 0x1771`
(= 6001) 을 돌려주고 있었다.

수정: ROM 호출 직전에 `address - 0x60000000` 변환 한 줄 추가.

---

## 1. 증상 (Initial Symptom)

OTA A/B Bootloader 의 C-2.3 마일스톤("runtime metadata flash write") 검증 도중,
gtkterm 시리얼 출력에서 다음과 같은 패턴이 반복됨:

```
=================================
 OTA Bootloader C-1 Started
=================================
[BOOT] Selected slot: A
[BOOT] Selected base: 0x60040000
[BOOT] Jumping to app...

[APP] 🟢 Reset_Handler Started in Flash!
[APP] ✅ SDRAM is Ready!

[APP-A-SDRAM] main entered
[APP-A-SDRAM] bootctrl read OK
[APP-A-SDRAM] bootctrl.magic          = 0x424F4F54
[APP-A-SDRAM] bootctrl.active_slot    = 0x00000000
[APP-A-SDRAM] bootctrl.pending_slot   = 0xFFFFFFFF
[APP-A-SDRAM] bootctrl.boot_success   = 0x00000001
[APP-A-SDRAM] bootctrl.boot_attempts  = 0x00000000
[APP-A-SDRAM] Phase B precheck start
[APP-A-SDRAM] Phase B precheck OK
[APP-A-SDRAM] Phase B write attempt start
[APP-A-SDRAM] write: shadow prepared?
[APP-A-SDRAM] write: before erase
[APP-A-SDRAM] Phase B confirm write FAILED (expected before real flash driver)
[APP-A-SDRAM] dbg = 0x0000E206
[APP-A-SDRAM] dbg_status = 0x00001771
[APP-A-SDRAM] relocation done, heartbeat start
[APP-A-SDRAM] heartbeat
[APP-A-SDRAM] heartbeat
```

부트 프레임 / Slot 점프 / SDRAM bring-up / metadata read / Phase B precheck 까지는
정상이지만, 마지막 한 점 — **runtime 에서 boot_ctrl 영역 (`0x603F0000`)에
metadata 를 다시 쓰는 단계의 ROM erase API 가 fail** 하는 상태.

## 2. 코드 흐름 (정상 동작 시 기대 흐름)

| 단계 | 위치 | 기대 출력 |
|---|---|---|
| ROM | BootROM (사용자 코드 X) | (출력 없음) FCB read → IVT read → bootloader 점프 |
| Bootloader Reset_Handler | `shared/src/startup_MIMXRT1020.c` `IS_BOOTLOADER` 분기 | (출력 없음) SystemInit/MPU/UART/data/bss |
| Bootloader main | `bootloader/src/main.c:45` | `OTA Bootloader C-1 Started` 배너 |
| 메타 read | `boot_policy.c` `Boot_GetCtrl/Boot_SelectSlot` | `Selected slot: A`, `base: 0x60040000` |
| App jump | `Jump_To_App(0x60040000)` | `Jumping to app...` |
| Slot A Reset_Handler | `shared/src/startup_MIMXRT1020.c` `else` 분기 | `Reset_Handler Started in Flash!`, SDRAM init, 재배치, VTOR |
| Slot A main | `app_slot_a/src/main.c:41` | `main entered`, bootctrl 출력, Phase B precheck/write 결과 |
| Heartbeat 루프 | `app_slot_a/src/main.c:92` | `heartbeat` 무한 반복 |

Phase B write 단계 (실패 지점) 의 정상 기대 출력:

```
write: shadow prepared?
write: before erase
write: after erase             ← 여기까지 못 가고 있었음
write: before program
write: after program
write: before verify
write: after verify
Phase B confirm write SUCCESS (unexpected at this stage)
```

## 3. 디버깅 가설과 검증

### Step 1 — Flash 칩 부품 / FCB tag 확인

- 보드: MIMXRT1020-EVK 의 Winbond W25Q64JV (8 MB, 1.8 V QSPI NOR)
- `0x60000000` 의 첫 4 바이트 = `0x42464346` ('FCFB') 정상

→ FCB 자체는 정상 인식.

### Step 2 — Sector erase opcode 변경

`shared/src/flexspi_nor_config.c` 의 LUT[20] 변경:

```c
/* Before */
[4 * 5 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0xD7, RADDR_SDR, PAD_1, 0x18),
/* After */
[4 * 5 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x20, RADDR_SDR, PAD_1, 0x18),
```

근거: `0xD7` 은 Atmel/AT45 dataflash 의 page erase opcode. W25Q64JV 의 4 KB
sector erase 는 `0x20` (W25Q64JV datasheet, "Sector Erase (4 KB) — 20h").

결과: **`dbg_status = 0x1771` 그대로**. opcode 가 root cause 아님.

### Step 3 — FCB version 정정

```c
/* Before */
.version = 0x56010400,   /* SDK newer */
/* After */
.version = 0x56010100,   /* RT1020 RM Table 9-15 (p.207) */
```

근거: RT1020 RM Table 9-15 의 Version 필드는 `0x56010000 / 0x56010100` 만 명시.
`0x56010400` 은 newer SDK 예제값이며 RT1020 ROM 의 in-application init 에서 strict
하게 검사할 가능성이 있어 RM 명시값으로 맞춤.

결과: **`dbg_status = 0x1771` 그대로**. version 도 root cause 아님.

### Step 4 — deviceModeCfg (Quad Enable) 추가

`shared/src/flexspi_nor_config.c` 에 ROM init 시 chip 의 QE bit 을 set 하는
시퀀스 추가:

```c
.deviceModeCfgEnable = 1,
.deviceModeType      = 1,                /* 1 = Quad Enable type */
.waitTimeCfgCommands = 1,
.deviceModeSeq       = 0x00000401u,      /* seqNum=1, seqId=4 */
.deviceModeArg       = 0x00000200u,      /* SR1=0x00, SR2=0x02 (= QE bit 1) */

/* LUT 추가 */
[4 * 4 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x01, WRITE_SDR, PAD_1, 0x02),
```

결과: **`dbg_status = 0x1771` 그대로**. QE 설정도 root cause 아님 (boot read 가
이미 잘 되고 있던 것 → QE 는 어디선가 이미 set 되어 있었음).

### Step 5 — STS0 진단 (★ 결정적 단서)

`app_slot_a/src/bootctrl_runtime_flash_lowlevel.c` 의 `BootCtrl_LowLevel_EraseSector`
ROM 호출 직전에 다음을 추가:

```c
#define BOOTCTRL_FLEXSPI_STS0   (*(volatile uint32_t *)0x402A80E0U)
#define BOOTCTRL_STS0_ARBIDLE   (1u << 1)

/* erase 직전 컨트롤러 상태 스냅샷 + ARBIDLE 폴링 */
g_bootctrl_dbg_sts0_before = BOOTCTRL_FLEXSPI_STS0;

if (api->flexSpiNorDriver->clear_cache) {
    api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
}
BootCtrl_DsbIsb();

uint32_t wait = 0;
while ((BOOTCTRL_FLEXSPI_STS0 & BOOTCTRL_STS0_ARBIDLE) == 0u) {
    if (++wait > 1000000u) break;
}
g_bootctrl_dbg_arbidle_wait = wait;
```

새 디버그 변수를 main.c 에서 같이 출력하도록 추가.

새 시리얼 결과:

```
[APP-A-SDRAM] dbg = 0x0000E206
[APP-A-SDRAM] dbg_status = 0x00001771
[APP-A-SDRAM] dbg_sts0_before = 0x00000003   ← !!!
[APP-A-SDRAM] dbg_arbidle_wait = 0x00000000
```

**`STS0 = 0x3`** = bit0 (`SEQIDLE` = 1) + bit1 (`ARBIDLE` = 1) = **컨트롤러는 완벽하게 idle**.

이 결과로 다음 가설들이 모두 부정됨:

- ❌ AHB prefetch 가 ROM 호출 시점에 진행 중이라 컨트롤러가 busy
- ❌ 인터럽트나 다른 master 가 FlexSPI 영역을 read 하고 있어 busy
- ❌ 이전 IP 명령이 정리되지 않아 busy

→ `0x1771` 은 **하드웨어 상태가 아니라 ROM 의 입력 검증 / 파라미터 거절** 단계에서
나오는 코드라는 결론.

### Step 6 — Address 파라미터 절대→상대 변환 (★ 실제 fix)

#### RM 근거

RT1020 RM **p.265, 9.13.2 FlexSPI NOR APIs example**:

```c
uint32_t address = 0x40000;                            // 256 KB (relative offset)
uint32_t sector_size = 0x1000;
status = flexspi_nor_flash_erase(instance, &config, address, sector_size);
status = flexspi_nor_flash_page_program(instance, &config, address, page_buffer);
...
uint32_t mem_address = 0x6000_0000 + address;          // XIP read 만 절대주소
```

즉 ROM API 의 `address` 파라미터는 **flash base (`0x6000_0000`) 로부터의 byte
offset**. XIP memcpy 같은 memory-mapped read 만 절대주소 사용.

#### 사용자 코드의 잘못된 호출 체인

```
boot_policy.h:15
    #define BOOT_CTRL_ADDR             0x603F0000u                   ← 절대주소
        ↓
bootctrl_runtime_flash.c:44
    #define BOOTCTRL_FLASH_BASE_ADDR   (BOOT_CTRL_ADDR)              ← 절대주소
        ↓
bootctrl_runtime_flash.c:215
    BootCtrl_FlashErase_Sector(BOOTCTRL_FLASH_BASE_ADDR);            ← 0x603F0000
        ↓
bootctrl_runtime_flash_lowlevel.c:226
    api->...->erase(0, &cfg, address, 4096);                         ← 0x603F0000 ❌
```

ROM 입장에서 `0x603F0000` 은 "flash base + 1.6 GB offset" → 8 MB flash 사이즈를
초과 → 입력 거절 → `status = 0x1771`.

#### 적용한 수정

`app_slot_a/src/bootctrl_runtime_flash_lowlevel.c`:

매크로 추가:

```c
#define BOOTCTRL_FLEXSPI_AMBA_BASE  (0x60000000u)
```

`BootCtrl_LowLevel_EraseSector` 의 ROM 호출 부분:

```c
/* 변경 전 */
status_t st = api->flexSpiNorDriver->erase(
    BOOTCTRL_FLEXSPI_INST,
    &g_bootctrl_flash_cfg_ram,
    address,                              /* = 0x603F0000 */
    BOOTCTRL_SECTOR_SIZE);

/* 변경 후 */
uint32_t flash_offset = address - BOOTCTRL_FLEXSPI_AMBA_BASE;   /* = 0x003F0000 */
status_t st = api->flexSpiNorDriver->erase(
    BOOTCTRL_FLEXSPI_INST,
    &g_bootctrl_flash_cfg_ram,
    flash_offset,
    BOOTCTRL_SECTOR_SIZE);
```

`BootCtrl_LowLevel_ProgramPage` 의 ROM 호출 부분도 동일하게 `address -
BOOTCTRL_FLEXSPI_AMBA_BASE` 로 변환.

`BootCtrl_LowLevel_Read` (XIP memcpy) 와 `BootCtrl_Verify` 는 절대주소 그대로 유지.

#### 결과

빌드 → flash → reset 후 시리얼:

```
[APP-A-SDRAM] write: shadow prepared?
[APP-A-SDRAM] write: before erase
[APP-A-SDRAM] write: after erase
[APP-A-SDRAM] write: before program
[APP-A-SDRAM] write: after program
[APP-A-SDRAM] write: before verify
[APP-A-SDRAM] write: after verify
[APP-A-SDRAM] Phase B confirm write SUCCESS (unexpected at this stage)
[APP-A-SDRAM] relocation done, heartbeat start
[APP-A-SDRAM] heartbeat
[APP-A-SDRAM] heartbeat
...
```

**C-2.3 마일스톤 완료**.

## 4. 0x1771 의 의미에 대한 정리

| 해석 시도 | 값 | 매칭 여부 |
|---|---|---|
| `(group * 100) + code` (NXP MCUXpresso `MAKE_STATUS`) | `60 * 100 + 1 = 6001 = 0x1771` → `kStatus_FLEXSPI_Busy` | 수치는 맞으나 RT1020 RM Table 9-49 에는 미등재 |
| `(group << 8) + code` | `0x17 * 0x100 + 0x71 = 0x1771` → group 23, code 113 | 표준 group 매핑에 없음 |
| RT1020 RM Table 9-49 (p.265-266) 명시 코드들 | 7000/7001/7002, 20100~20110 | 모두 불일치 |

→ **공식 RM 표에 0x1771 은 없으며, ROM 내부의 입력 검증 단계에서 vendor-specific 으로
돌려주는 reject 코드일 가능성이 높음**. 의미는 결국 "입력 파라미터를 받아들일 수 없음"
이고, 이번 케이스에선 address 가 flash 사이즈 범위 밖이라는 것이 실제 원인.

> **교훈**: ROM 이 돌려주는 status 가 RM 표에 없다고 해서 곧장 "controller busy" 라고
> 단정하면 안 된다. STS0 같은 하드웨어 레지스터로 컨트롤러 상태를 직접 확인해
> hardware 가설을 부정한 뒤, 입력 파라미터 가설로 옮겨가는 순서가 안전하다.

## 5. RM 페이지 빠른 참조 (이번 디버깅에서 사용)

| 항목 | 섹션 | 페이지 |
|---|---|---|
| FlexSPI Configuration Block (FCB) 필드 | 9.6.3.1, Table 9-15 | 207 ~ 209 |
| LUT sequence index 정의 (NOR) | Table 9-16 | 209 ~ 210 |
| ROM API 구조체 | 9.13 | 261 ~ 264 |
| ROM API NOTE 3, 4 (RWW / IRQ 규칙) | 9.13.2 | 262 ~ 263 |
| FlexSPI NOR API status 표 | Table 9-49 | 265 ~ 266 |
| **ROM API address 파라미터 example (★ root cause 의 근거)** | **9.13.2 example code** | **265** |
| LUT structure / instruction set | 27.4.6, Table 27-5 | 1414 ~ 1416 |
| Command Arbitration | 27.4.12 / 27.4.12.1 | 1443 ~ 1446 |
| FlexSPI 레지스터 맵 (base = 0x402A_8000) | 27.8.2.1 | 1470 ~ 1471 |
| AHBCR (PREFETCHEN / CACHEABLEEN) | 27.8.2.5 | 1476 ~ 1478 |
| **STS0 (ARBIDLE / SEQIDLE)** | **27.8.2.24** | **1503** |

## 6. 사이드 이슈 메모 (지금 풀 필요 없음)

### 6-1. heartbeat 간격이 예상보다 느림

`delay_loop(30000000)` × 2 = 6 천만 nop 반복.
132 MHz Cortex-M7 이면 이론적으로 ~7 초가 기대치이지만 측정값은 **12 ~ 30 초**.

원인 추정:
- I-cache / D-cache 비활성 (`SCB_EnableICache/DCache` 호출 없음)
- `delay_loop` 가 `.text` → SDRAM 에 배치되어 instruction fetch 가 매번 SEMC 를 통함
- `volatile uint32_t i` 라 컴파일러가 매 iteration 마다 reload/store

해결 방향(나중에): cache enable, 또는 `delay_loop` 를 `.ramfunc` (ITCM) 로 이동.

### 6-2. UART 출력의 `0x0x` prefix 중복

```
bootctrl.magic          = 0x0x424F4F54
```

`UART1_SendString("0x")` 다음에 `UART1_SendHex32` 가 또 내부에서 `"0x"` 를 붙이는
표시 버그. 동작에는 영향 없으나 출력 가독성 차원에서 정리 가능.

### 6-3. ARBIDLE 폴링 루프의 변수 갱신 위치

Step 5 에서 추가한 폴링:

```c
while ((BOOTCTRL_FLEXSPI_STS0 & BOOTCTRL_STS0_ARBIDLE) == 0u) {
    wait++;
    if (wait > 1000000u) break;
    g_bootctrl_dbg_arbidle_wait = wait;          /* break 다음에 있어서 timeout 시 갱신 안 됨 */
}
```

`g_bootctrl_dbg_arbidle_wait = wait;` 를 while 바깥으로 빼면 루프 한 번도 안 돌거나
timeout 으로 빠져나간 경우에도 마지막 wait 값이 남는다. 디버깅 잡힘새 개선 정도의
사소한 정리.

## 7. 다음 마일스톤 후보 (참고)

이번 C-2.3 완료 이후 자연스럽게 이어지는 다음 단계:

- **C-2.4** — `pending_slot=B` 로 metadata 갱신 + reset → bootloader 가 trial boot
  으로 Slot B 점프 → boot_success=YES 다시 마킹 → rollback 시나리오 검증.
- **Slot B 를 dummy → real SDRAM-reloc app 으로 교체** — 현재 `IS_DUMMY_APP` 매크로
  로 dummy 임. 둘 다 real 이면 진짜 OTA 시나리오.
- **boot_attempts / pending → active 전환 로직 채우기** — Phase A precheck 가
  trial boot 의 attempts 를 실제로 증가시키도록.

## 8. 변경 파일 요약

| 파일 | 변경 |
|---|---|
| `shared/src/flexspi_nor_config.c` | LUT[20] sector erase opcode `0xD7 → 0x20`, FCB version `0x56010400 → 0x56010100`, deviceModeCfg/QE LUT 추가 (Step 2/3/4 — 직접적 fix 아님) |
| `app_slot_a/src/bootctrl_runtime_flash_lowlevel.c` | STS0 진단 / clear_cache / ARBIDLE 폴링 추가 (Step 5), **address 절대→상대 변환 추가 (Step 6 = 실제 fix)** |
| `app_slot_a/src/main.c` | Step 5 의 `g_bootctrl_dbg_sts0_before` / `g_bootctrl_dbg_arbidle_wait` 출력 추가 |

## 9. 핵심 교훈

1. **RM 의 example code 는 시그니처 설명만큼 중요하다.** 이번 root cause 는 prototype
   에는 단순히 `uint32_t start` 라고만 되어 있었고, 실제 의미("flash base 로부터의
   offset")는 example 의 `uint32_t address = 0x40000;` 선언으로만 드러났다.
2. **하드웨어 상태와 status 코드는 다른 정보다.** 0x1771 만 보고 SDK 컨벤션상의
   "FLEXSPI Busy" 로 추정해 prefetch / IRQ 가설로 시간을 썼지만, 실제로는 STS0 = 0x3
   (idle) 이었다. 하드웨어 레지스터 직접 폴링이 결정적이었다.
3. **XIP read 와 ROM API 는 주소 규약이 다르다.** XIP 는 절대주소, ROM API 는 flash
   base offset. 같은 변수 이름(`address`)을 쓰면서 두 규약을 섞으면 이번 같은 bug 가
   생긴다. 매크로 이름에 `_AMBA_` (= memory-mapped) / `_OFFSET_` 을 붙여 구분하면
   안전.
