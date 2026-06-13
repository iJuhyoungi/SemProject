#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "rt1020_regs.h"


/**
 * ISR과 main이 공유하기 때문에 반드시 volatile로 선언
 */
static volatile const uint8_t *g_tx_buf;
static volatile uint32_t g_tx_len;
static volatile uint32_t g_tx_idx;
static volatile uint32_t g_tx_done;
static volatile uint32_t g_irq_count;               // ISR이 몇 번 불렸는지 확인

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

static void LPSPI1_Clock_Enable(void)
{
    CCM_CCGR1 = (CCM_CCGR1 & ~(0x3u << 0)) | (0x3u << 0); // LPSPI1 클럭 게이팅 해제
}

static void LPSPI1_Pin_Init(void)
{
    /* 4개 핀을 ALT1(=LPSPI1 기능)으로 mux. MUX_MODE[2:0]=001 */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_10 = 0x1; /*SCK*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_11 = 0x1; /*PCS0*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12 = 0x1; /*SDO*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13 = 0x1; /*SDI*/

    /*SDI는 입력으로 설정*/
    IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_02 = 0x1; /*SDI 선택 입력 레지스터, 0: LPSPI1_SDI, 1: GPIO*/
}

static void LPSPI1_Master_Init(void)
{
    /*soft reset pulse*/
    LPSPI1_CR = (1 << 1); // RST bit set, 소프트웨어 리셋 트리거
    LPSPI1_CR = 0;        // RST bit 클리어, 리셋 완료

    /*master mode 설정*/
    LPSPI1_CFGR1 = (1 << 0); // master mode

    /**
     * SCK period = (SCKDIV+2)*(funcclk/prescale)
     * funcclk=132MHz, prescale=1 -> SCKDIV=130 -> SCK=1MHz
     */
    LPSPI1_CCR = (130 << 0);

    /**
     * TXWATER=3: TX FIFO count <= 3 으로 떨어지면 TDF set (refill 신호).
     * IRQ의 refill 트리거
     */
    LPSPI1_FCR = (3u << 0); // TX FIFO 클리어

    /*module enable*/
    LPSPI1_CR = (1 << 0); // Enable bit set, 모듈 활성화
}

/* return : 0 = 정상 완료, 1 = timeout(전송 미완료) */
static int LPSPI1_Send_Byte(uint8_t b)
{
    LPSPI1_SR = (1u << 10);

    while (!(LPSPI1_SR & (1u << 0)))
    {
    }

    LPSPI1_TCR = (1u << 19) | (7u << 0);
    LPSPI1_TDR = b;

    /* 전송 완료 대기 - 무한 hang 방지용 bounded wait */
    uint32_t timeout = 1000000u;
    while (!(LPSPI1_SR & (1u << 10)) && --timeout)
    {
    }
    return (timeout == 0u) ? 1 : 0;
}

static int LPSPI1_Send_Buffer(const uint8_t *buf, uint32_t n, uint32_t *peak_out)
{
    LPSPI1_SR = (1u << 10);              /*stale TCF clear*/
    LPSPI1_TCR = (1u << 19) | (7u << 0); /*8bit, RXMSK*/

    uint32_t peak = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        // /*FIFO가 가득 차면 자리가 날때까지 반복하고, 그 외에는 계속 push*/
        // while ((LPSPI1_FSR & 0x1Fu) >= 16u)
        // {

        // }
        LPSPI1_TDR = buf[i];
        // uint32_t cnt = LPSPI1_FSR & 0x1Fu; // push 직후 TX FIFO 점유량
        // if (cnt > peak)
        //     peak = cnt;
    }

    uint32_t snap=LPSPI1_FSR&0x1Fu;
    if(peak_out)*peak_out=snap;

    uint32_t timeout = 2000000u;
    while (!(LPSPI1_SR & (1u << 10)) && --timeout)
    {
    }
    return (timeout == 0u) ? 1 : 0;
}

static void dwt_init(void)
{
    CM_DEMCR |= (1u << 24);         // TRCENA : DWT 모듈 enable
    DWT_CYCCNT = 0u;
    DWT_CTRL |= (1 << 0);           // CYCCNTENA : 사이클 count 시작
}

/**
 * 인터럽트 기반 송신 시작, 즉시 리턴하고 전송은 ISR이 백그라운드로 진행
 */
static void LPSPI1_Send_IRQ(const uint8_t *buf, uint32_t n)
{
    g_tx_buf = buf;
    g_tx_len = n;
    g_tx_idx = 0;
    g_tx_done = 0;
    g_irq_count = 0;

    LPSPI1_SR = (1u << 10);              // stale TCF clear
    LPSPI1_TCR = (1u << 19) | (7u << 0); // 8bit, RXMSK(one time)

    LPSPI1_IER = (1u << 0); // TDIE: TX FIFO가 watermark까지 비면 인터럽트 발생
    NVIC_ISER1 = (1u << 0); // IRQ 32 enable(32/32=1, 32%32=0)
}

void LPSPI1_IRQHandler(void)
{
    g_irq_count++;

    while (((LPSPI1_FSR & 0x1Fu) < 16u) && (g_tx_idx < g_tx_len))
    {
        LPSPI1_TDR = g_tx_buf[g_tx_idx++];
    }

    /**
     * 다 보냈다면, TDF 인터럽트 disable
     * TDF는 W1C가 아니라 FIFO가 empty 되면 지속적으로 enable되는 level 플래그
     * disable되지 않으면 FIFO가 empty 되자마자 또 인터럽트가 발생하여 무한 인터럽트 issue 발생
     */
    if (g_tx_idx >= g_tx_len)
    {
        LPSPI1_IER = 0u;
        g_tx_done = 1;
    }
}

int main(void)
{
    /* UART1은 startup의 IS_BOOTLOADER 분기가 이미 init 함 — 바로 출력 가능 */
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[PERI] RT1020 peripheral bring-up\r\n");
    UART1_SendString("[PERI] hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    LPSPI1_Clock_Enable(); // LPSPI1 클럭 활성화
    LPSPI1_Pin_Init();     // LPSPI1 핀 설정
    UART1_SendString("[PERI] LPSPI1 pins muxed (ALT1)\r\n");

    LPSPI1_Master_Init();
    UART1_SendString("[PERI] LPSPI1 CFGR1 = ");
    UART1_SendHex32(LPSPI1_CFGR1); /* 기대 0x00000001 */
    UART1_SendString("\r\n[PERI] LPSPI1 CCR   = ");
    UART1_SendHex32(LPSPI1_CCR); /* 기대 0x00000082 (=130) */
    UART1_SendString("\r\n[PERI] LPSPI1 SR    = ");
    UART1_SendHex32(LPSPI1_SR); /* bit0(TDF) set 기대 */
    UART1_SendString("\r\n");

    /* PARAM으로 FIFO 깊이 확인 */
    UART1_SendString("[PERI] PARAM = ");
    UART1_SendHex32(LPSPI1_PARAM);
    UART1_SendString("\r\n");

    static const uint8_t buf[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    UART1_SendString("[PERI] TX 0xA5 ...\r\n");
    int fail = LPSPI1_Send_Byte(0xA5);
    UART1_SendHex32(LPSPI1_FSR);
    UART1_SendString(fail ? "\r\n[PERI] TX TIMEOUT (전송 미완료)\r\n"
                          : "\r\n[PERI] TX OK (TCF set)\r\n");

    uint32_t peak=0;
    UART1_SendString("[PERI] burst 16 bytes ...\r\n");

    fail = LPSPI1_Send_Buffer(buf, 16, &peak);
    UART1_SendString("[PERI] FSR after fill loop = ");
    UART1_SendHex32(LPSPI1_FSR);
    UART1_SendString("\r\n");


    UART1_SendString("[PERI] FIFO peak during burst = ");
    UART1_SendHex32(peak);
    UART1_SendString(fail ? "\r\n[PERI] BURST TIMEOUT\r\n"
                          : "\r\n[PERI] BURST OK (TCF set)\r\n");

    UART1_SendString("[PERI] LPSPI1 VERID = ");
    UART1_SendHex32(LPSPI1_VERID);
    UART1_SendString("\r\n");

    dwt_init();

    static uint8_t big[40];
    for(uint32_t i=0;i<40;++i)big[i]=(uint8_t)i;

    /*polling방식*/
    DWT_CYCCNT=0u;
    LPSPI1_Send_Buffer(big,16,&(uint32_t){0});      // 16개(tight버전 n<=16)
    uint32_t cyc_poll=DWT_CYCCNT;

    /*IRQ방식 - Kick-off만 하고 즉시 리턴*/
    DWT_CYCCNT=0u;
    LPSPI1_Send_IRQ(big,40);                        // 40개, 백그라운드 전송
    uint32_t cyc_irq_kick=DWT_CYCCNT;               // main이 묶여있는 시간은 kick-off뿐

    /*그 사이 main운 자유로우며, 일부러 다른 task를 하며 전송이 끝나는지 확인*/
    uint32_t work=0;
    while(!g_tx_done){                              // 전송과 병렬로 루프를 수행한 count
        work++;
    }
    UART1_SendString("[PERI] poll cycles = ");
    UART1_SendHex32(cyc_poll);
    UART1_SendString("\r\n[PERI] irq kick cyc  = ");
    UART1_SendHex32(cyc_irq_kick);
    UART1_SendString("\r\n[PERI] irq count     = ");
    UART1_SendHex32(g_irq_count);
    UART1_SendString("\r\n[PERI] main work loops= ");
    UART1_SendHex32(work);
    UART1_SendString("\r\n");


    uint32_t beat = 0;
    while (1)
    {
        LED_On();
        delay_busy(50000000);
        LED_Off();
        delay_busy(50000000);

        UART1_SendString("[PERI] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}