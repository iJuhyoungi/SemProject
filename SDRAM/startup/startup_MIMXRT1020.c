#include <stdint.h>
#include "system_MIMXRT1020.h"

#define SCB_VTOR   (*(volatile uint32_t *)0xE000ED08)
#define SCB_CPACR  (*(volatile uint32_t *)0xE000ED88)

extern int main(void);

extern uint32_t __data_load_start__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __StackTop;

extern uint32_t __ramfunc_load_start__;
extern uint32_t __ramfunc_start__;
extern uint32_t __ramfunc_end__;

/* 데이터 포인터와 함수 포인터를 동시에 담을 수 있는 공용체 (ISO C 호환) */
typedef union {
    void (*handler)(void);
    uintptr_t value;
} vector_entry_t;

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)      __attribute__((weak, alias("Default_Handler")));

extern void LPUART1_IRQHandler(void);

/* ISO C 호환성을 위한 매크로 정의 (160개의 외부 인터럽트를 깔끔하게 채움) */
#define D_1  { .handler = Default_Handler }
#define D_10 D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1
#define D_100 D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10

/* NXP system_MIMXRT1020.c 파일이 찾을 수 있도록 이름 복구 (__VECTOR_TABLE) */
__attribute__((used, section(".isr_vector")))
const vector_entry_t __VECTOR_TABLE[176] = {
    { .value = (uintptr_t)&__StackTop }, /* 0: Initial Stack Pointer */
    { .handler = Reset_Handler },        /* 1: Reset Vector */
    { .handler = NMI_Handler },          /* 2: NMI */
    { .handler = HardFault_Handler },    /* 3: Hard Fault */
    { .handler = MemManage_Handler },    /* 4: Memory Management Fault */
    { .handler = BusFault_Handler },     /* 5: Bus Fault */
    { .handler = UsageFault_Handler },   /* 6: Usage Fault */
    { .value = 0 },                      /* 7: Reserved */
    { .value = 0 },                      /* 8: Reserved */
    { .value = 0 },                      /* 9: Reserved */
    { .value = 0 },                      /* 10: Reserved */
    { .handler = SVC_Handler },          /* 11: SVCall */
    { .handler = DebugMon_Handler },     /* 12: Debug Monitor */
    { .value = 0 },                      /* 13: Reserved */
    { .handler = PendSV_Handler },       /* 14: PendSV */
    { .handler = SysTick_Handler },      /* 15: SysTick */
    /* 🚀 16 ~ 175: External Interrupts 정밀 맵핑 */
    D_10, D_10,                                  /* 16 ~ 35 (IRQ 0 ~ 19) */
    { .handler = LPUART1_IRQHandler },           /* 36: IRQ 20 (LPUART1) 🚀 여기에 장착! */
    D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, /* 37 ~ 45 (IRQ 21 ~ 29) */
    D_100, D_10, D_10, D_10                      /* 46 ~ 175 (IRQ 30 ~ 159) */
};

void Reset_Handler(void)
{
    uint32_t *src = &__data_load_start__;
    uint32_t *dst = &__data_start__;
    uint32_t *ram_src = &__ramfunc_load_start__;
    uint32_t *ram_dst = &__ramfunc_start__;

    SCB_VTOR = (uint32_t)__VECTOR_TABLE;
    SCB_CPACR |= (0xF << 20);

    /* 1) .data 복사 */
    while (dst < &__data_end__) {
        *dst++ = *src++;
    }

    /* 2) .ramfunc 복사 */
    while (ram_dst < &__ramfunc_end__) {
        *ram_dst++ = *ram_src++;
    }
    /* 3) .bss 초기화 */
    for (dst = &__bss_start__; dst < &__bss_end__;) {
        *dst++ = 0U;
    }

    SystemInit();
    (void)main();

    for (;;) {
    }
}

void Default_Handler(void)
{
    for (;;) {
    }
}
