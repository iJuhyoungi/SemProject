#include "Cfg.h"

const Gpt_ConfigType Gpt_Config = {
    .prescaler=23u,             /* GPT1_PR: ÷(23+1) */
    .period_ticks=1000000u,     /* GPT1_OCR1 */
};

/* RTWDOG: TOVAL 0xFFFF ≈ 2.048초 @32kHz LPO (16-bit 한계, P-11 교훈).
   WIN 0x2000 ≈ 0.25초 = HW window mode ON — 허용 refresh 창 [0.25s, 2.048s).
   WdgM feed 케이던스(≈0.4초) > WIN 이라 정상 흐름은 항상 통과 */
const Wdg_ConfigType Wdg_Config = {
    .timeout_ticks = 0xFFFFu,
    .window_ticks  = 0x2000u,
};

const Pwm_ConfigType Pwm_Config = {
    .period=0xFFFFu,            /* SM0VAL1 = 주기 top */
    .initial_duty=0x4000u,      /* 표준 0x8000=100% 스케일 (SWS PwmDutycycleDefault) */
};

const Adc_ConfigType Adc_Config = {
    .group0_channel=0x19u,      /* 그룹0 -> 물리 채널 매핑 (0x19=VREFSH 내부채널) */
};

const Can_ConfigType Can_Config = {
    .presdiv=63u,               /* CTRL1[31:24] 프리스케일러 (loopback 데모라 baud 자유) */
    .loopback=1u,               /* CTRL1[12] loopback enable */
};

/* LPSPI1: SCK = 132MHz/(130+2) ≈ 1MHz. 주의: CCR SCKDIV 필드는 8-bit (최대 255,
   min SCK ≈ 514kHz @prescale1) — 초과값은 옆 필드(DBT)로 넘쳐 오동작 (TOVAL 16-bit 교훈과 동일) */
const Spi_ConfigType Spi_Config = {
    .sckdiv = 130u
};

const Icu_ConfigType Icu_Config = {
    .gen_half_period=0x0FFFu
};

const WdgM_ConfigType WdgM_Config = {
    .expected_min     = 1u,
    .expected_max     = 6u,
    .failed_tolerance = 1u,
};

