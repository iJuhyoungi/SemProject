#include "Gpt.h"
#include "rt1020_regs.h"

#define GPT_EN (1u << 0)
#define GPT_ENMOD (1u << 1)
#define GPT_CLKSRC_IPG (1u << 6)
#define GPT_SWR (1u << 15)
#define GPT_OF1 (1u << 0)

volatile uint32_t g_gpt_tick;

void Gpt1_Init(void)
{
    CCM_CCGR1 |= (3u << 20) | (3u << 22);

    GPT1_CR = GPT_SWR;
    while (GPT1_CR & GPT_SWR)
    {
    }

    GPT1_CR = GPT_CLKSRC_IPG | GPT_ENMOD;

    GPT1_PR = 23u;
    GPT1_OCR1 = 1000000u;

    g_gpt_tick = 0;

    GPT1_IR = GPT_OF1;
    NVIC_ISER3 = (1u << 4);   /* GPT IRQ(IRQ100) enable */

    GPT1_CR |= GPT_EN;
}

void GPT1_IRQHandler(void)
{
    if (GPT1_SR & GPT_OF1)
    {
        GPT1_SR = GPT_OF1; // W1C 클리어
        g_gpt_tick++;
    }
}

uint32_t Gpt1_GetCount(void)
{
    return GPT1_CNT;
}

uint32_t Gpt1_GetTick(void)
{
    return g_gpt_tick;
}

/* ===== AUTOSAR Wrapping ===== */
void Gpt_Init(const Gpt_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    Gpt1_Init();
}

Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel)
{
    (void)Channel;
    return GPT1_CNT;                  /* 이번 주기에서 경과한 tick */
}

Gpt_ValueType Gpt_GetTimeRemaining(Gpt_ChannelType Channel)
{
    (void)Channel;
    return GPT1_OCR1 - GPT1_CNT;      /* 목표(OCR1)까지 남은 tick */
}

void Gpt_EnableNotification(Gpt_ChannelType Channel)
{
    (void)Channel;
    GPT1_IR |= (1u << 0);             /* OF1IE */
}

void Gpt_DisableNotification(Gpt_ChannelType Channel)
{
    (void)Channel;
    GPT1_IR &= ~(1u << 0);
}

