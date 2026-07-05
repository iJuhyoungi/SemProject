#include <stdint.h>
#include "rt1020_regs.h"
#include "lpspi.h"

/* 내부 상태 (lpspi,c 안에서만 사용하므로 static을 유지) */
static volatile const uint8_t *g_tx_buf;
static volatile uint32_t g_tx_len;
static volatile uint32_t g_tx_idx;

/* main과 공유 */
volatile uint32_t g_tx_done;
volatile uint32_t g_irq_count;
volatile uint32_t g_dma_done;
volatile uint32_t g_dma_irq_count;

void LPSPI1_Clock_Enable(void)
{
    CCM_CCGR1 = (CCM_CCGR1 & ~(0x3u << 0)) | (0x3u << 0); // LPSPI1 클럭 게이팅 해제
}

void LPSPI1_Pin_Init(void)
{
    /* 4개 핀을 ALT1(=LPSPI1 기능)으로 mux. MUX_MODE[2:0]=001 */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_10 = 0x1; /*SCK*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_11 = 0x1; /*PCS0*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12 = 0x1; /*SDO*/
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13 = 0x1; /*SDI*/

    /*SDI는 입력으로 설정*/
    IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_02 = 0x1; /*SDI 선택 입력 레지스터, 0: LPSPI1_SDI, 1: GPIO*/
}

void LPSPI1_Master_Init(void)
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
int LPSPI1_Send_Byte(uint8_t b)
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

int LPSPI1_Send_Buffer(const uint8_t *buf, uint32_t n, uint32_t *peak_out)
{
    LPSPI1_SR = (1u << 10);              /*stale TCF clear*/
    LPSPI1_TCR = (1u << 19) | (7u << 0); /*8bit, RXMSK*/

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

/**
 * 인터럽트 기반 송신 시작, 즉시 리턴하고 전송은 ISR이 백그라운드로 진행
 */
void LPSPI1_Send_IRQ(const uint8_t *buf, uint32_t n)
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

void LPSPI1_Send_DMA(const uint8_t *buf, uint32_t n)
{
    g_dma_done = 0;
    g_dma_irq_count = 0;

    CCM_CCGR5 |= (3u << 6); // eDMA+DMAMUX 클럭 게이트 ON

    /* TCD 채널0번 config */
    EDMA_TCD0_SADDR = (uint32_t)buf;
    EDMA_TCD0_SOFF = 1;      /* source +1 byte 단위 */
    EDMA_TCD0_ATTR = 0x0000; /* 8-bit <-> 8-bit */
    EDMA_TCD0_NBYTES = 1;    /* minor loop = 요청당 1 byte */
    EDMA_TCD0_SLAST = 0;
    EDMA_TCD0_DADDR = (uint32_t)&LPSPI1_TDR; /* dest = TDR (고정) */
    EDMA_TCD0_DOFF = 0;
    EDMA_TCD0_CITER = (uint16_t)n; /* major loop = n */
    EDMA_TCD0_BITER = (uint16_t)n; /* 초기값 동일 */
    EDMA_TCD0_DLASTSGA = 0;
    EDMA_TCD0_CSR = (1u << 3) | (1u << 1); /* DREQ + INTMAJOR(완료 시 IRQ) ← 0x0A */

    /* DMAMUX : LPSPI1 TX(소스 14)를 채널 0에 연결하고 enable 시킴 */
    DMAMUX_CHCFG0 = (1u << 31) | 14u;

    /* LPSPI : 프레임 명령과 TX DMA 요청을 enable 시킴 */
    LPSPI1_SR = (1u << 10);              /* stale TCF clear */
    LPSPI1_TCR = (1u << 19) | (7u << 0); /* 8bit, RXMSK */
    LPSPI1_DER = (1u << 0);              /* TDDE: FIFO 비면 DMA 요청 */

    NVIC_ISER0 = (1u << 0); /* DMA0=IRQ0 enable */
    /* 채널0 요청 수신 ON = 전송 시작. 이후 CPU 개입 없이 eDMA 가 알아서 옮김. */
    EDMA_SERQ = 0; /* 채널 번호는 0 */
}

void DMA0_IRQHandler(void)
{
    EDMA_CINT = 0;              /* INT 플래그 클리어 */
    (void)EDMA_INT;             /* read-back: 클리어를 리턴 전에 반영 (재진입 방지, dma irq count 2 -> 1) */
    g_dma_irq_count++;
    g_dma_done = 1;
}

  void LPSPI1_Disable(void)
  {
      LPSPI1_CR = 0u;   /* MEN=0, 모듈 off */
  }

