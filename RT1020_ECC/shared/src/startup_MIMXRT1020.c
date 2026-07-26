#include <stdint.h>
#include "system_MIMXRT1020.h"
/* 헤더 파일 추가하여 경고 해결 */
#include "uart.h"
#include "clock.h"
#include "semc.h"

#define SCB_VTOR   (*(volatile uint32_t *)0xE000ED08)
#define SCB_CPACR  (*(volatile uint32_t *)0xE000ED88)

extern int main(void);

extern uint32_t __text_load_start__;
extern uint32_t __text_start__;
extern uint32_t __text_end__;

extern uint32_t __data_load_start__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __StackTop;

extern uint32_t __ramfunc_load_start__;
extern uint32_t __ramfunc_start__;
extern uint32_t __ramfunc_end__;

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
/* 프로젝트에서 정의하면 override 되는 weak 기본값 (FLS 는 아직 미사용) */
void LPSPI1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));   /* IRQ 32  -> idx 48 */
void DMA0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));   /* IRQ 0   -> idx 16 */
void GPT1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));   /* IRQ 100 -> idx 116 */

#define D_1  { .handler = Default_Handler }
#define D_10 D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1
#define D_100 D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10, D_10

__attribute__((used, section(".isr_vector")))
const vector_entry_t __VECTOR_TABLE[176] = {
    { .value = (uintptr_t)&__StackTop }, 
    { .handler = Reset_Handler },        
    { .handler = NMI_Handler },          
    { .handler = HardFault_Handler },    
    { .handler = MemManage_Handler },    
    { .handler = BusFault_Handler },     
    { .handler = UsageFault_Handler },   
    { .value = 0 },                      
    { .value = 0 },                      
    { .value = 0 },                      
    { .value = 0 },                      
    { .handler = SVC_Handler },          
    { .handler = DebugMon_Handler },     
    { .value = 0 },                      
    { .handler = PendSV_Handler },       
    { .handler = SysTick_Handler },      
    { .handler = DMA0_IRQHandler },      
    D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, 
    D_10,
    { .handler = LPUART1_IRQHandler },           
    D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, D_1, 
    D_1, D_1,
    { .handler=LPSPI1_IRQHandler },
      /* idx49~115 = IRQ33~99 : 67개 (D_10×6 + D_1×7) */
      D_10, D_10, D_10, D_10, D_10, D_10, D_1, D_1, D_1, D_1, D_1, D_1, D_1,
      { .handler = GPT1_IRQHandler },   /* idx116 = IRQ100 */
      /* idx117~148 : 32개 (원래 D_100의 나머지, D_10×3 + D_1×2) */
      D_10, D_10, D_10, D_1, D_1,
      /* idx149~175 : 27개 (원래 꼬리, D_10×2 + D_1×7) */
      D_10, D_10, D_1, D_1, D_1, D_1, D_1, D_1, D_1

};

__attribute__((section(".boot_text"))) void Reset_Handler(void)
{
    SystemInit();
    SCB_CPACR |= (0xF << 20); /* FPU 활성화 */

    /* I-cache 활성화: flash XIP 실행을 결정적/고속으로.
       캐시 없이 XIP 면 busy-loop 실행시간이 코드 정렬에 의존해 워치독 등
       타이밍 민감 코드가 레이아웃에 따라 오동작(Heisenbug). 여기서 켠다. */
    *(volatile uint32_t *)0xE000EF50 = 0u;            /* ICIALLU: I-cache 무효화 */
    __asm volatile ("dsb");
    __asm volatile ("isb");
    *(volatile uint32_t *)0xE000ED14 |= (1u << 17);   /* SCB_CCR.IC = 1 : I-cache enable */
    __asm volatile ("dsb");
    __asm volatile ("isb");

#if defined(IS_BOOTLOADER)

    UART1_Init();

    volatile uint32_t *src, *dst;

    src = (volatile uint32_t *)&__data_load_start__;
    dst = (volatile uint32_t *)&__data_start__;
    while (dst < (volatile uint32_t *)&__data_end__) { *dst++ = *src++; }

    /* ramfunc를 FLASH에서 ITCM으로 복사
     * (BootCtrl_LowLevel_* 등 flash erase 중에서도 fetch가 가능해야 하는 함수들)*/
    src = (volatile uint32_t * )&__ramfunc_load_start__;
    dst = (volatile uint32_t *)&__ramfunc_start__;

    while (dst < (volatile uint32_t *)&__ramfunc_end__) {
        *dst++ = *src++;
    }

    for (dst = (volatile uint32_t *)&__bss_start__;
         dst < (volatile uint32_t *)&__bss_end__;) {
        *dst++ = 0U;
    }

#elif defined(IS_DUMMY_APP)

    UART1_Init();

    volatile uint32_t *src, *dst;

    src = (volatile uint32_t *)&__data_load_start__;
    dst = (volatile uint32_t *)&__data_start__;
    while (dst < (volatile uint32_t *)&__data_end__) { *dst++ = *src++; }

    for (dst = (volatile uint32_t *)&__bss_start__;
         dst < (volatile uint32_t *)&__bss_end__;) {
        *dst++ = 0U;
    }

#else

    Clock_Init_132MHz();
    UART1_Init();
    UART1_SendString("\r\n\r\n[APP] 🟢 Reset_Handler Started in Flash!\r\n");

    SDRAM_Clock_Init();
    SDRAM_Pin_Init();
    if (SDRAM_Init_Sequence() == 0) {
        UART1_SendString("[APP] 🚨 FATAL: SDRAM Init Failed! System Halted.\r\n");
        while(1);
    }
    UART1_SendString("[APP] ✅ SDRAM is Ready!\r\n");

    volatile uint32_t *src, *dst;

    src = (volatile uint32_t *)&__text_load_start__;
    dst = (volatile uint32_t *)&__text_start__;
    while (dst < (volatile uint32_t *)&__text_end__) { *dst++ = *src++; }

    src = (volatile uint32_t *)&__data_load_start__;
    dst = (volatile uint32_t *)&__data_start__;
    while (dst < (volatile uint32_t *)&__data_end__) { *dst++ = *src++; }

    src = (volatile uint32_t *)&__ramfunc_load_start__;
    dst = (volatile uint32_t *)&__ramfunc_start__;
    while (dst < (volatile uint32_t *)&__ramfunc_end__) { *dst++ = *src++; }

    for (dst = (volatile uint32_t *)&__bss_start__;
         dst < (volatile uint32_t *)&__bss_end__;) {
        *dst++ = 0U;
    }

#endif

    SCB_VTOR = (uint32_t)__VECTOR_TABLE;
    (void)main();

    for (;;) {}
}
__attribute__((section(".boot_text"))) void Default_Handler(void)
{
    for (;;) {
    }
}
