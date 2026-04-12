// #include <stdint.h>
// #include "dma.h"
// #include "uart.h"
// #include "rt1020_regs.h"

// #define EDMA_TCD0_WORD ((volatile uint32_t *)(EDMA_BASE + 0x1000))

// static void DMA_DumpStatus(const char *tag)
// {
//     uint16_t csr   = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x1C));
//     uint16_t citer = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x16));
//     uint16_t biter = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x1E));

//     UART1_SendString(tag);
//     UART1_SendString("\r\n  CSR   = 0x");
//     UART1_SendHex32(csr);
//     UART1_SendString("\r\n  ES    = 0x");
//     UART1_SendHex32(EDMA_ES);
//     UART1_SendString("\r\n  ERQ   = 0x");
//     UART1_SendHex32(EDMA_ERQ);
//     UART1_SendString("\r\n  CITER = 0x");
//     UART1_SendHex32(citer);
//     UART1_SendString("\r\n  BITER = 0x");
//     UART1_SendHex32(biter);

//     UART1_SendString("\r\n  CSR.START  = ");
//     UART1_SendChar((csr & (1U << 0)) ? '1' : '0');

//     UART1_SendString("\r\n  CSR.ACTIVE = ");
//     UART1_SendChar((csr & (1U << 6)) ? '1' : '0');

//     UART1_SendString("\r\n  CSR.DONE   = ");
//     UART1_SendChar((csr & (1U << 7)) ? '1' : '0');

//     UART1_SendString("\r\n");
// }

// void DMA_Init(void)
// {
//     UART1_SendString("DMA_Init: enter\r\n");

//     /* clock gate on */
//     (*(volatile uint32_t *)0x400FC068) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC06C) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC070) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC074) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC078) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC07C) = 0xFFFFFFFF;
//     (*(volatile uint32_t *)0x400FC080) = 0xFFFFFFFF;

//     UART1_SendString("DMA_Init: clocks ok\r\n");

//     EDMA_CR = (1U << 3) | (1U << 2);   // ERGA=1, ERCA=1
//     UART1_SendString("DMA_Init: EDMA_CR ok\r\n");

//     DMAMUX_CHCFG0 = 0x00000000;
//     UART1_SendString("DMA_Init: DMAMUX ok\r\n");

//     EDMA_CERQ = 0;
//     UART1_SendString("DMA_Init: CERQ ok\r\n");

//     EDMA_CDNE = 0;
//     UART1_SendString("DMA_Init: CDNE ok\r\n");

//     EDMA_CERR = 0;
//     UART1_SendString("DMA_Init: CERR ok\r\n");

//     UART1_SendString("DMA_Init: exit\r\n");
// }

// void DMA_MemCopy(void *dst, void *src, uint32_t length)
// {
//     volatile uint32_t *tcd = EDMA_TCD0_WORD;

//     /* stale status clear */
//     EDMA_CERQ = 0;
//     EDMA_CDNE = 0x40;
//     EDMA_CERR = 0x40;

//     /* TCD 8 words clear */
//     for (int i = 0; i < 8; i++) {
//         tcd[i] = 0;
//     }

//     /*
//      * TCD word map
//      * 0x00 SADDR
//      * 0x04 ATTR[31:16] | SOFF[15:0]
//      * 0x08 NBYTES
//      * 0x0C SLAST
//      * 0x10 DADDR
//      * 0x14 CITER[31:16] | DOFF[15:0]
//      * 0x18 DLAST_SGA
//      * 0x1C BITER[31:16] | CSR[15:0]
//      */

//     tcd[0] = (uint32_t)src;
//     tcd[1] = ((uint32_t)0x0202 << 16) | (uint16_t)4;  // SSIZE=32bit, DSIZE=32bit, SOFF=4
//     tcd[2] = length;                                   // one minor loop = whole buffer
//     tcd[3] = 0;                                        // SLAST
//     tcd[4] = (uint32_t)dst;
//     tcd[5] = ((uint32_t)1 << 16) | (uint16_t)4;       // CITER=1, DOFF=4
//     tcd[6] = 0;                                        // DLAST_SGA
//     tcd[7] = ((uint32_t)1 << 16) | (uint16_t)0;       // BITER=1, CSR=0

//     __asm("dsb");
//     __asm("isb");

//     DMA_DumpStatus("Before SERQ/SSRT");

//     /* enable ch0 request path */
//     EDMA_SERQ = 0;

//     DMA_DumpStatus("After SERQ");

//     /* software start ch0 */
//     EDMA_SSRT = 0;

//     DMA_DumpStatus("After SSRT");

//     uint32_t timeout = 10000000;
//     uint32_t last_citer = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x16));

//     while (timeout > 0) {
//         uint16_t csr = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x1C));
//         uint16_t citer = *((volatile uint16_t *)(EDMA_BASE + 0x1000 + 0x16));

//         if (csr & (1U << 7)) {
//             UART1_SendString("DMA DONE\r\n");
//             DMA_DumpStatus("Final DONE status");
//             break;
//         }

//         if (EDMA_ES != 0) {
//             UART1_SendString("DMA ERROR\r\n");
//             DMA_DumpStatus("Final ERROR status");
//             break;
//         }

//         if (citer != last_citer) {
//             UART1_SendString("CITER changed to 0x");
//             UART1_SendHex32(citer);
//             UART1_SendString("\r\n");
//             last_citer = citer;
//         }

//         timeout--;
//     }

//     if (timeout == 0) {
//         UART1_SendString("DMA TIMEOUT\r\n");
//         DMA_DumpStatus("Final TIMEOUT status");
//     }

//     EDMA_CDNE = 0x40;
// }


#include <stdint.h>
#include "dma.h"
#include "uart.h"
#include "rt1020_regs.h"

typedef struct
{
    volatile uint32_t SADDR;      /* 0x00 */
    volatile uint16_t SOFF;       /* 0x04 */
    volatile uint16_t ATTR;       /* 0x06 */
    volatile uint32_t NBYTES;     /* 0x08 */
    volatile uint32_t SLAST;      /* 0x0C */
    volatile uint32_t DADDR;      /* 0x10 */
    volatile uint16_t DOFF;       /* 0x14 */
    volatile uint16_t CITER;      /* 0x16 */
    volatile uint32_t DLAST_SGA;  /* 0x18 */
    volatile uint16_t CSR;        /* 0x1C */
    volatile uint16_t BITER;      /* 0x1E */
} edma_tcd_t;

#define EDMA_TCD0   (*(volatile edma_tcd_t *)(EDMA_BASE + 0x1000U))

/* ATTR field helpers */
#define EDMA_ATTR_SIZE_32BIT   (0x2U)
#define EDMA_ATTR_VALUE(ssize, dsize)  (uint16_t)((((ssize) & 0x7U) << 8) | ((dsize) & 0x7U))

/* CSR field helpers */
#define EDMA_CSR_START_MASK    (1U << 0)
#define EDMA_CSR_ACTIVE_MASK   (1U << 6)
#define EDMA_CSR_DONE_MASK     (1U << 7)

static void DMA_DumpStatus(const char *tag)
{
    UART1_SendString(tag);

    UART1_SendString("\r\n  CSR   = 0x");
    UART1_SendHex32(EDMA_TCD0.CSR);

    UART1_SendString("\r\n  ES    = 0x");
    UART1_SendHex32(EDMA_ES);

    UART1_SendString("\r\n  ERQ   = 0x");
    UART1_SendHex32(EDMA_ERQ);

    UART1_SendString("\r\n  CITER = 0x");
    UART1_SendHex32(EDMA_TCD0.CITER);

    UART1_SendString("\r\n  BITER = 0x");
    UART1_SendHex32(EDMA_TCD0.BITER);

    UART1_SendString("\r\n  CSR.START  = ");
    UART1_SendChar((EDMA_TCD0.CSR & EDMA_CSR_START_MASK) ? '1' : '0');

    UART1_SendString("\r\n  CSR.ACTIVE = ");
    UART1_SendChar((EDMA_TCD0.CSR & EDMA_CSR_ACTIVE_MASK) ? '1' : '0');

    UART1_SendString("\r\n  CSR.DONE   = ");
    UART1_SendChar((EDMA_TCD0.CSR & EDMA_CSR_DONE_MASK) ? '1' : '0');

    UART1_SendString("\r\n");
}

static void DMA_ClearChannel0Status(void)
{
    EDMA_CERQ = 0;   /* disable request ch0 */
    EDMA_CDNE = 0;   /* clear DONE ch0 */
    EDMA_CERR = 0;   /* clear ERROR ch0 */
}

static void DMA_ConfigTCD0(void *dst, const void *src, uint32_t length)
{
    EDMA_TCD0.SADDR = (uint32_t)src;
    EDMA_TCD0.SOFF = 4;
    EDMA_TCD0.ATTR = EDMA_ATTR_VALUE(EDMA_ATTR_SIZE_32BIT, EDMA_ATTR_SIZE_32BIT);
    EDMA_TCD0.NBYTES = length;
    EDMA_TCD0.SLAST = 0;

    EDMA_TCD0.DADDR = (uint32_t)dst;
    EDMA_TCD0.DOFF = 4;
    EDMA_TCD0.CITER = 1;
    EDMA_TCD0.DLAST_SGA = 0;
    EDMA_TCD0.CSR = 0;
    EDMA_TCD0.BITER = 1;
}

void DMA_Init(void)
{
    UART1_SendString("DMA_Init: enter\r\n");

    /* clock gate on */
    (*(volatile uint32_t *)0x400FC068) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC06C) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC070) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC074) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC078) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC07C) = 0xFFFFFFFF;
    (*(volatile uint32_t *)0x400FC080) = 0xFFFFFFFF;

    UART1_SendString("DMA_Init: clocks ok\r\n");

    /* round-robin arbitration */
    EDMA_CR = (1U << 3) | (1U << 2);
    UART1_SendString("DMA_Init: EDMA_CR ok\r\n");

    /* software-start path */
    DMAMUX_CHCFG0 = 0x00000000;
    UART1_SendString("DMA_Init: DMAMUX ok\r\n");

    DMA_ClearChannel0Status();
    UART1_SendString("DMA_Init: clear status ok\r\n");

    UART1_SendString("DMA_Init: exit\r\n");
}

void DMA_MemCopy(void *dst, void *src, uint32_t length)
{
    uint32_t timeout = 10000000;
    uint16_t last_citer;

    DMA_ClearChannel0Status();

    /* TCD0 clear */
    EDMA_TCD0.SADDR = 0;
    EDMA_TCD0.SOFF = 0;
    EDMA_TCD0.ATTR = 0;
    EDMA_TCD0.NBYTES = 0;
    EDMA_TCD0.SLAST = 0;
    EDMA_TCD0.DADDR = 0;
    EDMA_TCD0.DOFF = 0;
    EDMA_TCD0.CITER = 0;
    EDMA_TCD0.DLAST_SGA = 0;
    EDMA_TCD0.CSR = 0;
    EDMA_TCD0.BITER = 0;

    DMA_ConfigTCD0(dst, src, length);

    __asm("dsb");
    __asm("isb");

    DMA_DumpStatus("Before SERQ/SSRT");

    EDMA_SERQ = 0;
    DMA_DumpStatus("After SERQ");

    EDMA_SSRT = 0;
    DMA_DumpStatus("After SSRT");

    last_citer = EDMA_TCD0.CITER;

    while (timeout > 0U)
    {
        uint16_t csr = EDMA_TCD0.CSR;
        uint16_t citer = EDMA_TCD0.CITER;

        if (csr & EDMA_CSR_DONE_MASK)
        {
            UART1_SendString("DMA DONE\r\n");
            DMA_DumpStatus("Final DONE status");
            break;
        }

        if (EDMA_ES != 0U)
        {
            UART1_SendString("DMA ERROR\r\n");
            DMA_DumpStatus("Final ERROR status");
            break;
        }

        if (citer != last_citer)
        {
            UART1_SendString("CITER changed to 0x");
            UART1_SendHex32(citer);
            UART1_SendString("\r\n");
            last_citer = citer;
        }

        timeout--;
    }

    if (timeout == 0U)
    {
        UART1_SendString("DMA TIMEOUT\r\n");
        DMA_DumpStatus("Final TIMEOUT status");
    }

    EDMA_CDNE = 0x40;
}
