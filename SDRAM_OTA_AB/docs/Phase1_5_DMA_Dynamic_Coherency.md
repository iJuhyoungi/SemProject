# NXP i.MX RT1020 Bare-Metal Firmware Project

## 🚀 Phase 1.5: eDMA & Advanced Cache Coherency (Dynamic Maintenance)

본 단계의 목표는 CPU의 개입 없이 메모리 간 데이터를 고속으로 전송하는 eDMA(Enhanced DMA) 드라이버를 베어메탈로 구현하고, L1 D-Cache가 활성화된 환경에서 필연적으로 발생하는 '캐시 불일치(Cache Coherency)' 문제를 하드웨어 레벨에서 완벽하게 통제하는 것입니다.

---

### 🕵️‍♂️ 트러블슈팅 1: NXP eDMA 컨트롤러의 함정들

NXP eDMA를 라이브러리 없이 직접 제어하는 과정에서 마주친 치명적인 하드웨어 맹점들과 해결책입니다.

1. **레지스터 오프셋 파편화 (The Residual State Trap)**
   * **증상:** DMA `START` 비트를 켜도 동작하지 않고 무한 대기(Hang)에 빠짐. 이전 작업의 에러(ERR)가 남아있어 트리거를 무시함.
   * **원인:** NXP 칩셋마다 8-bit 제어 레지스터의 위치가 다름. (RT1020의 경우 `0x028` 대역이 아닌 `0x014` 대역을 사용해야 함).
   * **해결:** 정확한 주소(`EDMA_CDNE = 0x014`, `EDMA_CERR = 0x016`)를 매핑하여 이전 작업의 잔해를 하드웨어적으로 완벽히 소각.
2. **DMAMUX 문지기 (The Gatekeeper)**
   * **원인:** eDMA는 아무리 소프트웨어 트리거를 발생시켜도, 앞단의 `DMAMUX`가 해당 채널의 경로를 열어주지 않으면 동작하지 않음.
   * **해결:** `DMAMUX_CHCFG0` 레지스터의 `ENBL` 비트를 1로 설정하여 채널 0번을 개방.
3. **AXI 버스 병목 및 멈춤 (Indivisible Transfer Crash)**
   * **증상:** 4096바이트를 한 번의 마이너 루프(`NBYTES = 4096`)로 전송 지시 시 칩 전체가 다운됨.
   * **원인:** AXI 버스에서 대용량 단일 전송(Indivisible operation)을 감당하지 못하고 Bus Fault 발생.
   * **해결:** DMAMUX의 **Always-On** 모드(`A_ON=1`)를 활용. 4096바이트를 한 번에 보내지 않고, 1번에 4바이트(`NBYTES=4`)씩 총 1024번(`CITER=1024`)으로 잘게 쪼개어 하드웨어가 스스로 페이스를 조절하며 복사하도록 구조 재설계.

---

### 🧠 트러블슈팅 2: Static vs Dynamic Cache Coherency

DMA와 CPU가 메모리를 공유할 때 캐시 불일치를 해결하는 두 가지 아키텍처를 모두 구현 및 검증했습니다.

#### 1. Static Buffer Allocation (정적 할당 / MPU 격리)
* **개념:** MPU를 사용해 SDRAM의 특정 영역(예: `0x81E00000`)을 `Non-Cacheable`로 지정하고, 링커 스크립트를 통해 DMA 전용 버퍼를 해당 섹션(`.dma_buffer`)에 고정.
* **장점:** C 코드에서 별도의 처리 불필요. 절대적으로 안전함.
* **단점:** 속도가 느린 메모리 영역을 강제로 써야 하며, 동적으로 생성되는 범용 데이터(Heap, 일반 전역 변수)에는 적용 불가.

#### 2. Dynamic Cache Maintenance (동적 캐시 제어) - 🌟 최종 채택
* **개념:** 버퍼를 최고 속도의 범용 Cacheable SDRAM(`.bss`)에 배치하되, DMA 전송 전후로 CPU가 캐시 라인을 직접 청소(Clean)하고 무효화(Invalidate)함.
* **동기화 흐름:**
  1. **[TX]** CPU Write ➔ **`Cache Clean`** (캐시 데이터를 SDRAM으로 푸시) ➔ DMA Transfer
  2. **[RX]** DMA Transfer ➔ **`Cache Invalidate`** (캐시를 파기하여 SDRAM에서 새로 로드) ➔ CPU Read
* **트러블슈팅 (SCS 레지스터 주소 오타):** Cortex-M7의 캐시 유지보수 레지스터는 `0xE000EDxx`가 아니라 **`0xE000EFxx`**에 위치함. 이를 수정하여 하드웨어 제어 성공.

---

### 💻 핵심 구현 코드 (Core Implementations)

#### 1. 베어메탈 캐시 유지보수 (src/cache.c)
```c
/* 🚀 정확한 Cortex-M7 특수 레지스터 주소 매핑 (EF 대역) */
#define SCB_DCCMVAC (*(volatile uint32_t *)0xE000EF68) /* D-Cache Clean by Address */
#define SCB_DCIMVAC (*(volatile uint32_t *)0xE000EF5C) /* D-Cache Invalidate by Address */

void DCache_CleanByAddr(void *addr, uint32_t size)
{
    uint32_t op_addr = (uint32_t)addr;
    int32_t op_size = (int32_t)size;

    /* 캐시 라인(32바이트) 경계 정렬 */
    uint32_t align_offset = op_addr & 0x1F; 
    op_addr -= align_offset;
    op_size += align_offset;

    __asm("dsb");
    while (op_size > 0) {
        SCB_DCCMVAC = op_addr; 
        op_addr += 32;
        op_size -= 32;
    }
    __asm("dsb");
    __asm("isb");
}

void DCache_InvalidateByAddr(void *addr, uint32_t size)
{
    uint32_t op_addr = (uint32_t)addr;
    int32_t op_size = (int32_t)size;

    uint32_t align_offset = op_addr & 0x1F;
    op_addr -= align_offset;
    op_size += align_offset;

    __asm("dsb");
    while (op_size > 0) {
        SCB_DCIMVAC = op_addr;
        op_addr += 32;
        op_size -= 32;
    }
    __asm("dsb");
    __asm("isb");
}
```

#### 2. 진정한 Dynamic Coherency 검증 시나리오 (src/main.c)
```c
/* 섹션 지시자 제거 -> 일반 Cacheable 영역(.bss)에 할당 
 * 캐시 라인 오염(Cacheline false sharing)을 막기 위해 32바이트 정렬 */
uint8_t dma_tx_buffer[4096] __attribute__((aligned(32)));
uint8_t dma_rx_buffer[4096] __attribute__((aligned(32)));

int main(void)
{
    // ... 시스템 초기화 및 DMA, Cache Enable ...

    /* 1. CPU가 TX 버퍼(L1 캐시)에 데이터 기록 */
    for (uint32_t i = 0; i < 4096; i++) {
        dma_tx_buffer[i] = (uint8_t)(i & 0xFF);
    }

    /* 🚀 2. Cache Clean: TX 버퍼의 데이터를 SDRAM으로 강제 푸시 */
    DCache_CleanByAddr(dma_tx_buffer, 4096);

    /* 3. eDMA 전송 수행 (하드웨어 복사) */
    DMA_MemCopy(dma_rx_buffer, dma_tx_buffer, 4096);

    /* 🚀 4. Cache Invalidate: RX 버퍼의 L1 캐시를 무효화하여 최신 SDRAM 데이터 동기화 */
    DCache_InvalidateByAddr(dma_rx_buffer, 4096);

    /* 5. 데이터 무결성 검증 (PASS) */
    // ...
}
```

---

### 🔜 Next Target: [Option A] OTA Bootloader & A/B Rollback
메모리 최적화와 시스템 버스, DMA 제어가 완벽해졌습니다. 이제 시스템의 안정성을 보장하기 위해 8MB Flash를 분할하고, 런타임 펌웨어 업데이트(OTA) 및 복구(Rollback)를 수행하는 2-stage Bootloader 아키텍처를 구축합니다.

