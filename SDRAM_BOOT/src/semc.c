// #include "semc.h"
// #include "rt1020_regs.h"

// /* Step 1-1: SEMC 클럭 공급 */
// void SDRAM_Clock_Init(void) {
//     /* 1. SEMC 모듈 클럭 활성화 (CCGR3, CG2) 
//      * 비트 4~5를 11(3)로 설정하여 모듈을 켭니다. */
//     CCM_CCGR3 |= (3 << 4);

//     /* 💡 클럭 분주(Divider) 생략의 이유:
//      * 우리는 이미 이전 프로젝트에서 메인 시스템 버스(periph_clk)를 
//      * 가장 안정적인 132MHz로 세팅해 두었습니다. 
//      * SEMC는 기본적으로 이 periph_clk를 그대로 받아 쓰도록(1분주) 
//      * Boot ROM에 의해 초기 설정되어 있으므로, 
//      * 복잡한 PLL 분주비 변경 없이 이대로 사용하면 132MHz로 완벽하게 동작합니다! */
// }

// /* Step 1-2: 40개 핀 다중화(Pin Muxing) 노가다 우회 */
// void SDRAM_Pin_Init(void) {
//     /* 시작 주소를 32비트 포인터로 캐스팅 */
//     // IOMUXC_* : 해당 물리적 핀을 어떤 모듈이, 어떤 전기적 특성으로 사용할지에 대한 모든 하드웨어적 설정을 담당
//     volatile uint32_t *emc_mux_base = (volatile uint32_t *)IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00;           // 핀의 목적지(MUX)만 연결

//     /* 🚀 2. PAD 레지스터 베이스 주소 (전기적 파워 스위치) 추가! */
//     volatile uint32_t *emc_pad_base = (volatile uint32_t *)0x401F8188;                                  // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC 베이스 레지스터 주소

//     /* * GPIO_EMC_00 ~ GPIO_EMC_41 까지 총 42개의 핀을 
//      * 한 번에 ALT0 (SEMC 기능)으로 설정합니다.
//      * (NXP RT1020은 EMC 핀들의 ALT0가 모두 SEMC로 할당되어 있습니다)
//      */
//     for (int i = 0; i < 42; i++) {
//         emc_mux_base[i] = 0x00;         // ALT0 모드 적용
//         emc_pad_base[i] = 0x0110B9;     //  IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_00 description 참고
//     }

//     /* * (참고) 현업에서는 전기적 특성(Drive Strength, Pull-up 등)을 조절하는 
//      * PAD Control 레지스터도 세팅하지만, EVK 보드의 기본 하드웨어 설계가 
//      * 훌륭하여 Boot ROM의 기본값(Default)만으로도 132MHz 통신은 충분히 버팁니다.
//      */

//     /* ==========================================================
//      * 🚀 핵심 해결책: DQS 루프백 핀(GPIO_EMC_28) 강제 입력 개방 (SION)
//      * ========================================================== */
//     // RT1020의 진짜 DQS 핀은 28번입니다!
//     // ALT0(0x00)에 SION 비트(0x10)를 OR 연산하여 덮어씁니다.
//     emc_mux_base[28] = 0x10;
// }

// /* 🚀 내부 헬퍼 함수 업그레이드: IPTXDAT 파라미터 추가! */
// static int SDRAM_SendCommand(uint32_t cmd, uint32_t arg, uint32_t txdat) {
//     SEMC_IPCR0 = arg; 
//     SEMC_IPCR1 = 2;   
//     SEMC_IPCR2 = 0;   
    
//     /* 🚀 핵심: MRS 명령(0x0A)의 Mode Register 값을 전달하기 위한 데이터 세팅 */
//     SEMC_IPTXDAT = txdat;

//     SEMC_INTR = 0x03; 
//     SEMC_IPCMD = 0xA55A0000 | cmd; 
    
//     uint32_t timeout = 1000000;
//     while (!(SEMC_INTR & 0x01)) { 
//         if (--timeout == 0) {
//             UART1_SendString("\r\n[SEMC H/W Error] Command Timeout!\r\n");
//             return 0; 
//         }
//     }
    
//     if (SEMC_INTR & 0x02) {
//         UART1_SendString("\r\n[SEMC H/W Error] Command Rejected by Controller!\r\n");
//         SEMC_INTR = 0x03; 
//         return 0; 
//     }
    
//     SEMC_INTR = 0x03; 
//     return 1; 
// }

// /* 🚀 Step 2 업그레이드: 구간별 에러 로그 출력 및 중단 로직 */
// int SDRAM_Init_Sequence(void) {
//     /* 🚀 DQS 모드 명확화: 28번 핀 SION 개방과 세트인 DQS 패드 루프백(0x04) 사용 */
//     SEMC_MCR = 0x10000004;          
    
//     SEMC_BR0 = 0x80000000 | (0x0D << 1) | 0x01;
// // 🚀 수정됨: CL=3, COL=9, BL=8, 16-bit에 대한 완벽한 TRM 매칭 값 (0x0F31)
//     SEMC_SDRAMCR0 = 0x00000F31;
//     SEMC_SDRAMCR1 = 0x00772A22; 
//     SEMC_SDRAMCR2 = 0x00010A0D; 
//     SEMC_SDRAMCR3 = 0x50210A08; 

//     uint32_t sdram_base = 0x80000000;

//     /* 명령 전송 시 세 번째 인자(txdat)로 0을 넘김 (MRS 제외) */
//     if (!SDRAM_SendCommand(0x08, sdram_base, 0)) return 0;
//     if (!SDRAM_SendCommand(0x09, sdram_base, 0)) return 0;
//     if (!SDRAM_SendCommand(0x09, sdram_base, 0)) return 0;

//     /* ==========================================================
//      * 🚀 주형님의 완벽한 한 방: MRS 명령 시 IPTXDAT에 0x33 전달!
//      * ========================================================== */
//     if (!SDRAM_SendCommand(0x0A, sdram_base, 0x33)) {
//         UART1_SendString(" -> ❌ [Error] Mode Register Set (0x0A) Failed\r\n");
//         return 0;
//     }

//     SEMC_SDRAMCR3 |= (1 << 31); 
//     return 1; 
// }
// /* 🚀 Step 3: SDRAM Read/Write 무결성 가혹 테스트 */
// void SDRAM_Test(void) {
//     /* SDRAM 시작 주소 (우리가 맵핑한 0x80000000) */
//     volatile uint32_t *sdram = (volatile uint32_t *)0x80000000;
    
//     /* 전체 32MB 중 앞부분 4MB (1,000,000 개의 32비트 Word) 영역을 가혹하게 검증 */
//     uint32_t test_words = 1000000; 
//     uint32_t i;

//     UART1_SendString("\r\n=================================\r\n");
//     UART1_SendString("[3] SDRAM Integrity Test Start...\r\n");
//     UART1_SendString("=================================\r\n");

//     /* 1. Write Phase: 배열의 인덱스 번호 자체를 데이터로 기록 
//      * (이 방식은 Address Bus가 꼬였거나 단락되었을 때 즉각 잡아낼 수 있습니다) */
//     UART1_SendString(" -> Writing 1,000,000 Words...\r\n");
//     for (i = 0; i < test_words; i++) {
//         sdram[i] = i; 
//     }
//     UART1_SendString(" -> Write Phase Done. Verifying Data...\r\n");

//     /* 2. Read & Verify Phase: 쓴 데이터가 1비트의 손상도 없이 남아있는지 확인 */
//     for (i = 0; i < test_words; i++) {
//         uint32_t read_val = sdram[i];
//         if (read_val != i) {
//             /* 데이터가 깨졌거나, 타이밍이 안 맞아서 밀렸을 경우 즉각 정지 */
//             UART1_SendString(" -> ❌ FATAL ERROR: VERIFY FAILED!\r\n");
//             /* 예상 값과 읽은 값 직접 출력 */
//             UART1_SendString(" -> Expected Index (Data) : ");
//             UART1_SendHex32(i);
            
//             UART1_SendString("\r\n -> Actually Read Value   : ");
//             UART1_SendHex32(read_val);
//             UART1_SendString("\r\n");

//             return; // 실패 시 즉시 함수 종료
//         }
//     }
    
//     /* 여기까지 단 한 번의 if문에도 걸리지 않고 도달했다면 완벽한 것입니다! */
//     UART1_SendString(" -> ✅ VERIFY PASSED! SDRAM is 100% Reliable!\r\n");
// }


#include <stdint.h>
#include "semc.h"
#include "uart.h"
#include "rt1020_regs.h"

/* =========================
 * Fixed config
 * ========================= */
#define SDRAM_BASE_ADDR              (0x80000000u)

/* BR0: 32MB + valid */
#define SEMC_BR_MS_32MB              (0x0Du << 1)
#define SEMC_BR_VLD                  (0x1u)

/* SDRAMCR0: CL=3, COL=9, BL=8, PS=16-bit */
#define SDRAMCR0_CL_3                (0x3u << 10)
#define SDRAMCR0_COL_9               (0x3u << 8)
#define SDRAMCR0_BL_8                (0x3u << 4)
#define SDRAMCR0_PS_16BIT            (0x1u << 0)

#define SEMC_SDRAMCR0_CFG \
    (SDRAMCR0_CL_3 | SDRAMCR0_COL_9 | SDRAMCR0_BL_8 | SDRAMCR0_PS_16BIT)

/* SDRAM Mode Register: BL=8, sequential, CL=3 */
#define SDRAM_MR_BL8_CL3             (0x33u)

/* IPCMD command codes from RT1020 TRM Table 25-2 */
#define SEMC_IPCMD_SDRAM_MRS             (0x0Au)
#define SEMC_IPCMD_SDRAM_AUTO_REFRESH    (0x0Cu)
#define SEMC_IPCMD_SDRAM_PRECHARGE_ALL   (0x0Fu)

#define SEMC_INTR_IPCMDDONE          (1u << 0)
#define SEMC_INTR_IPCMDERR           (1u << 1)
#define SEMC_INTR_CLEAR_ALL          (SEMC_INTR_IPCMDDONE | SEMC_INTR_IPCMDERR)

/* SDRAMCR3 fields */
#define SEMC_SDRAMCR3_REN            (1u << 3)
#define SEMC_SDRAMCR3_REBL_MASK      (0x7u << 4)

/* =========================
 * Local delay
 * ========================= */
__attribute__((section(".boot_text")))
 static void semc_delay(volatile uint32_t n)
{
    while (n--) {
        __asm("nop");
    }
}

/* =========================
 * Step 1-1: SEMC clock
 * ========================= */
__attribute__((section(".boot_text")))
void SDRAM_Clock_Init(void)
{
    /* CCM_CCGR3 CG2: enable SEMC clock */
    CCM_CCGR3 |= (3u << 4);
}

/* =========================
 * Step 1-2: Pin mux
 * ========================= */
__attribute__((section(".boot_text")))
void SDRAM_Pin_Init(void)
{
    volatile uint32_t *emc_mux_base =
        (volatile uint32_t *)IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00;

    /* GPIO_EMC_00 ~ GPIO_EMC_41 => ALT0(SEMC) */
    for (int i = 0; i < 42; i++) {
        emc_mux_base[i] = 0x00u;
    }

    /* GPIO_EMC_28 => SEMC_DQS + SION */
    emc_mux_base[28] = 0x10u;
}

/* =========================
 * IP command helper
 * ========================= */
__attribute__((section(".boot_text")))
static int SDRAM_SendCommand(uint32_t cmd, uint32_t addr, uint32_t txdat)
{
    SEMC_IPCR0   = addr;
    SEMC_IPCR1   = 2u;
    SEMC_IPCR2   = 0u;
    SEMC_IPTXDAT = txdat;

    SEMC_INTR = SEMC_INTR_CLEAR_ALL;
    SEMC_IPCMD = 0xA55A0000u | cmd;

    uint32_t timeout = 1000000u;
    while ((SEMC_INTR & SEMC_INTR_IPCMDDONE) == 0u) {
        if (--timeout == 0u) {
            UART1_SendString("\r\n[SEMC] IPCMD timeout\r\n");
            return 0;
        }
    }

    if (SEMC_INTR & SEMC_INTR_IPCMDERR) {
        UART1_SendString("\r\n[SEMC] IPCMD error\r\n");
        SEMC_INTR = SEMC_INTR_CLEAR_ALL;
        return 0;
    }

    SEMC_INTR = SEMC_INTR_CLEAR_ALL;
    return 1;
}

/* =========================
 * Step 2: SDRAM init
 * ========================= */
__attribute__((section(".boot_text")))
int SDRAM_Init_Sequence(void)
{
    /* DQS mode / mapping / geometry
     * NOTE:
     * 0x10000004 유지. 이 값은 현재 보드/프로젝트 기준 경험값이며,
     * SDRAM 칩 데이터시트 없이는 더 강하게 단정하기 어렵습니다.
     */
    SEMC_MCR      = 0x10000004u;
    SEMC_BR0      = SDRAM_BASE_ADDR | SEMC_BR_MS_32MB | SEMC_BR_VLD;
    SEMC_SDRAMCR0 = SEMC_SDRAMCR0_CFG;
    SEMC_SDRAMCR1 = 0x00772A22u;
    SEMC_SDRAMCR2 = 0x00010A0Du;
    SEMC_SDRAMCR3 = 0x50210A08u;

    /* init 동안 auto refresh off */
    SEMC_SDRAMCR3 &= ~SEMC_SDRAMCR3_REN;

    /* init 때 refresh burst = 1 */
    SEMC_SDRAMCR3 &= ~SEMC_SDRAMCR3_REBL_MASK;

    /* power-up settle */
    semc_delay(20000u);

    /* JEDEC-style init on RT1020 SEMC:
     * 1) PRECHARGE ALL
     * 2) AUTO REFRESH
     * 3) AUTO REFRESH
     * 4) MODE REGISTER SET
     */
    if (!SDRAM_SendCommand(SEMC_IPCMD_SDRAM_PRECHARGE_ALL, SDRAM_BASE_ADDR, 0u)) {
        UART1_SendString(" -> FAIL: PRECHARGE ALL\r\n");
        return 0;
    }

    if (!SDRAM_SendCommand(SEMC_IPCMD_SDRAM_AUTO_REFRESH, SDRAM_BASE_ADDR, 0u)) {
        UART1_SendString(" -> FAIL: AUTO REFRESH #1\r\n");
        return 0;
    }

    if (!SDRAM_SendCommand(SEMC_IPCMD_SDRAM_AUTO_REFRESH, SDRAM_BASE_ADDR, 0u)) {
        UART1_SendString(" -> FAIL: AUTO REFRESH #2\r\n");
        return 0;
    }

    /* MRS value must go through IPTXDAT */
    if (!SDRAM_SendCommand(SEMC_IPCMD_SDRAM_MRS, SDRAM_BASE_ADDR, SDRAM_MR_BL8_CL3)) {
        UART1_SendString(" -> FAIL: MODE REGISTER SET\r\n");
        return 0;
    }

    /* normal refresh enable */
    SEMC_SDRAMCR3 |= SEMC_SDRAMCR3_REN;

    semc_delay(2000u);
    return 1;
}

/* =========================
 * Step 3-1: 16-bit test
 * ========================= */
int SDRAM_Test16(void)
{
    volatile uint16_t *sdram16 = (volatile uint16_t *)(SDRAM_BASE_ADDR + 0x01000000);
    const uint32_t test_words = 1024u;

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString("[3] SDRAM 16-bit Integrity Test\r\n");
    UART1_SendString("=================================\r\n");

    for (uint32_t i = 0; i < test_words; i++) {
        sdram16[i] = (uint16_t)i;
    }

    for (uint32_t i = 0; i < test_words; i++) {
        uint16_t v = sdram16[i];
        if (v != (uint16_t)i) {
            UART1_SendString(" -> FAIL (16-bit)\r\n");
            UART1_SendString(" -> Index    : ");
            UART1_SendHex32(i);
            UART1_SendString("\r\n -> Expected : ");
            UART1_SendHex32((uint32_t)(uint16_t)i);
            UART1_SendString("\r\n -> Actual   : ");
            UART1_SendHex32((uint32_t)v);
            UART1_SendString("\r\n");
            return 0;
        }
    }

    UART1_SendString(" -> PASS (16-bit)\r\n");
    return 1;
}

/* =========================
 * Step 3-2: 32-bit test
 * ========================= */
int SDRAM_Test32(void)
{
    volatile uint32_t *sdram32 = (volatile uint32_t *)(SDRAM_BASE_ADDR + 0x01000000);
    const uint32_t test_words = 1024u;

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString("[4] SDRAM 32-bit Integrity Test\r\n");
    UART1_SendString("=================================\r\n");

    for (uint32_t i = 0; i < test_words; i++) {
        sdram32[i] = i;
    }

    for (uint32_t i = 0; i < test_words; i++) {
        uint32_t v = sdram32[i];
        if (v != i) {
            UART1_SendString(" -> FAIL (32-bit)\r\n");
            UART1_SendString(" -> Index    : ");
            UART1_SendHex32(i);
            UART1_SendString("\r\n -> Expected : ");
            UART1_SendHex32(i);
            UART1_SendString("\r\n -> Actual   : ");
            UART1_SendHex32(v);
            UART1_SendString("\r\n");
            return 0;
        }
    }

    UART1_SendString(" -> PASS (32-bit)\r\n");
    return 1;
}
