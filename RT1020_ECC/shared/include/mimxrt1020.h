#ifndef MIMXRT1020_H_
#define MIMXRT1020_H_

#include <stdint.h>

#define __CM7_REV            0x0000U
#define __MPU_PRESENT        1U
#define __NVIC_PRIO_BITS     4U
#define __Vendor_SysTickConfig 0U
#define __FPU_PRESENT        1U
#define __VTOR_PRESENT       1U

typedef enum IRQn {
    NonMaskableInt_IRQn   = -14,
    HardFault_IRQn        = -13,
    MemoryManagement_IRQn = -12,
    BusFault_IRQn         = -11,
    UsageFault_IRQn       = -10,
    SVCall_IRQn           = -5,
    DebugMonitor_IRQn     = -4,
    PendSV_IRQn           = -2,
    SysTick_IRQn          = -1,
} IRQn_Type;

typedef struct {
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR;
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint8_t SHP[12];
    volatile uint32_t SHCSR;
    volatile uint32_t CFSR;
    volatile uint32_t HFSR;
    volatile uint32_t DFSR;
    volatile uint32_t MMFAR;
    volatile uint32_t BFAR;
    volatile uint32_t AFSR;
    volatile uint32_t PFR[2];
    volatile uint32_t DFR;
    volatile uint32_t ADR;
    volatile uint32_t MMFR[4];
    volatile uint32_t ISAR[5];
    uint32_t RESERVED0[5];
    volatile uint32_t CPACR;
} SCB_Type;

#define SCS_BASE             (0xE000E000UL)
#define SCB_BASE             (SCS_BASE + 0x0D00UL)
#define SCB                  ((SCB_Type *)SCB_BASE)

#endif

