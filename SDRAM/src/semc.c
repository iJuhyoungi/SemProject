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


/* =============================================================================
 * semc.c  (활성 코드)
 * -----------------------------------------------------------------------------
 * SEMC(Smart External Memory Controller)로 외부 SDRAM 을 초기화하고 검증한다.
 * 흐름: 클럭 게이팅 -> 핀 MUX -> SEMC 레지스터 설정 -> JEDEC 초기화 -> R/W 테스트.
 *
 * (위쪽의 주석 처리된 블록은 옛 버전 — 학습 기록용 보존)
 * ============================================================================= */
#include <stdint.h>
#include "semc.h"
#include "uart.h"
#include "rt1020_regs.h"

/* =========================
 * Fixed config
 * ========================= */
#define SDRAM_BASE_ADDR              (0x80000000u)   /* SDRAM 이 매핑될 CPU 주소 */

/* BR0(Base Register): SDRAM 의 주소/크기 매핑.
 *   MS 필드(비트 4:1) = 0x0D -> 메모리 크기 32MB
 *   VLD 비트(비트 0)  = 1    -> 이 매핑을 유효화 */
#define SEMC_BR_MS_32MB              (0x0Du << 1)
#define SEMC_BR_VLD                  (0x1u)

/* SDRAMCR0: SDRAM 칩의 구조/타이밍 파라미터.
 *   CL=3(CAS Latency), COL=9(컬럼 주소 9비트), BL=8(Burst Length), PS=16비트 데이터폭.
 *   주의: 여기 CL 값과 아래 Mode Register 의 CL 은 반드시 일치해야 한다. */
#define SDRAMCR0_CL_3                (0x3u << 10)
#define SDRAMCR0_COL_9               (0x3u << 8)
#define SDRAMCR0_BL_8                (0x3u << 4)
#define SDRAMCR0_PS_16BIT            (0x1u << 0)

#define SEMC_SDRAMCR0_CFG \
    (SDRAMCR0_CL_3 | SDRAMCR0_COL_9 | SDRAMCR0_BL_8 | SDRAMCR0_PS_16BIT)

/* SDRAM Mode Register 값: BL=8, sequential burst, CL=3.
 * MRS 명령 때 IPTXDAT 로 SDRAM 칩에 전달된다. */
#define SDRAM_MR_BL8_CL3             (0x33u)

/* IPCMD 명령 코드 — SEMC 가 SDRAM 에 보내는 명령 (RT1020 TRM Table 25-2). */
#define SEMC_IPCMD_SDRAM_MRS             (0x0Au)   /* Mode Register Set */
#define SEMC_IPCMD_SDRAM_AUTO_REFRESH    (0x0Cu)   /* Auto Refresh */
#define SEMC_IPCMD_SDRAM_PRECHARGE_ALL   (0x0Fu)   /* Precharge All */

/* INTR(상태) 레지스터 비트 — IP 명령 완료/에러 플래그. */
#define SEMC_INTR_IPCMDDONE          (1u << 0)     /* 명령 완료 */
#define SEMC_INTR_IPCMDERR           (1u << 1)     /* 명령 에러 */
#define SEMC_INTR_CLEAR_ALL          (SEMC_INTR_IPCMDDONE | SEMC_INTR_IPCMDERR)

/* SDRAMCR3 필드 — refresh 동작 제어. */
#define SEMC_SDRAMCR3_REN            (1u << 3)     /* Refresh Enable */
#define SEMC_SDRAMCR3_REBL_MASK      (0x7u << 4)   /* Refresh Burst Length 필드 마스크 */

/* =========================
 * Local delay
 * ========================= */
/* nop 루프 기반 단순 지연. n 은 volatile 이라 루프가 최적화로 사라지지 않는다.
 * (정밀 타이머가 아니므로 SDRAM 파워업 settle 같은 곳에서 여유 있게 잡아야 함) */
static void semc_delay(volatile uint32_t n)
{
    while (n--) {
        __asm("nop");
    }
}

/* =========================
 * Step 1-1: SEMC clock
 * ========================= */
/* SEMC 모듈 클럭을 켠다 (clock gating).
 * CCGR3 의 CG2 필드(비트 5:4)를 11(=3)로 -> SEMC 항상 ON.
 * 클럭 소스/분주(CSCMR1)는 Boot ROM 기본값에 의존 — periph_clk 그대로 사용 가정. */
void SDRAM_Clock_Init(void)
{
    /* CCM_CCGR3 CG2: enable SEMC clock */
    CCM_CCGR3 |= (3u << 4);
}

/* =========================
 * Step 1-2: Pin mux
 * ========================= */
/* SDRAM 에 연결된 EMC 핀들을 SEMC 기능으로 묶는다.
 * 주의: 여기서는 MUX(기능)만 설정하고 PAD(드라이브 강도/슬루레이트)는
 *       Boot ROM 기본값에 의존한다 — 고속 동작 신호 무결성 점검 시 1순위 확인 대상. */
void SDRAM_Pin_Init(void)
{
    /* EMC_00 의 MUX 레지스터 주소를 베이스로 잡는다.
     * EMC 핀들의 MUX 레지스터는 4바이트 간격으로 연속 배치되어 있어 배열처럼 접근 가능. */
    volatile uint32_t *emc_mux_base =
        (volatile uint32_t *)IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00;

    /* GPIO_EMC_00 ~ GPIO_EMC_41 (42개) => 모두 ALT0(=SEMC 기능).
     * RT1020 에서 EMC 핀의 ALT0 가 SEMC 로 할당돼 있어 0 만 써넣으면 된다. */
    for (int i = 0; i < 42; i++) {
        emc_mux_base[i] = 0x00u;
    }

    /* GPIO_EMC_28 => SEMC_DQS + SION.
     * 0x10 = ALT0(하위 3비트=0, 즉 DQS 기능) + SION 비트(비트4).
     * SION 으로 출력 핀을 입력으로도 되읽어(loopback) 리드 타이밍을 잡는다. */
    emc_mux_base[28] = 0x10u;
}

/* =========================
 * IP command helper
 * ========================= */
/* SEMC 의 IP 명령 인터페이스로 SDRAM 에 명령 1개를 보내고 완료를 기다린다.
 *   cmd   : 명령 코드 (PRECHARGE/REFRESH/MRS 등)
 *   addr  : 명령 대상 주소
 *   txdat : 명령에 실어 보낼 데이터 (MRS 의 모드 레지스터 값 등, 없으면 0)
 * 반환: 1 성공 / 0 실패(타임아웃 또는 에러).  */
static int SDRAM_SendCommand(uint32_t cmd, uint32_t addr, uint32_t txdat)
{
    SEMC_IPCR0   = addr;     /* 명령 대상 주소 */
    SEMC_IPCR1   = 2u;       /* 데이터 크기 등 명령 파라미터 */
    SEMC_IPCR2   = 0u;
    SEMC_IPTXDAT = txdat;    /* 함께 전송할 데이터 (MRS 값 등) */

    SEMC_INTR = SEMC_INTR_CLEAR_ALL;       /* 이전 완료/에러 플래그 클리어 */
    /* IPCMD 에 쓰면 명령 실행 시작. 상위 0xA55A 는 오작동 방지용 키. */
    SEMC_IPCMD = 0xA55A0000u | cmd;

    /* 완료(IPCMDDONE) 플래그를 폴링. 무한 대기를 막기 위해 timeout 카운터 사용. */
    uint32_t timeout = 1000000u;
    while ((SEMC_INTR & SEMC_INTR_IPCMDDONE) == 0u) {
        if (--timeout == 0u) {
            UART1_SendString("\r\n[SEMC] IPCMD timeout\r\n");
            return 0;
        }
    }

    /* 완료됐어도 에러 플래그가 섰으면 명령이 거부된 것 — 실패 처리. */
    if (SEMC_INTR & SEMC_INTR_IPCMDERR) {
        UART1_SendString("\r\n[SEMC] IPCMD error\r\n");
        SEMC_INTR = SEMC_INTR_CLEAR_ALL;
        return 0;
    }

    SEMC_INTR = SEMC_INTR_CLEAR_ALL;       /* 다음 명령을 위해 플래그 정리 */
    return 1;
}

/* =========================
 * Step 2: SDRAM init
 * ========================= */
int SDRAM_Init_Sequence(void)
{
    /* DQS mode / mapping / geometry
     * NOTE:
     * 0x10000004 유지. 이 값은 현재 보드/프로젝트 기준 경험값이며,
     * SDRAM 칩 데이터시트 없이는 더 강하게 단정하기 어렵습니다.
     */
    SEMC_MCR      = 0x10000004u;   /* SEMC 마스터 설정: DQS 패드 루프백 모드 등 (경험값) */
    SEMC_BR0      = SDRAM_BASE_ADDR | SEMC_BR_MS_32MB | SEMC_BR_VLD;  /* 0x80000000, 32MB, 유효 */
    SEMC_SDRAMCR0 = SEMC_SDRAMCR0_CFG;   /* CL=3, COL=9, BL=8, 16비트 */
    /* CR1~CR3: tRAS/tRP/tRC/tRFC/refresh 주기 등 타이밍 묶음.
     * 현재 매직 넘버 — 실장 SDRAM 칩 데이터시트 기준으로 재계산 권장. */
    SEMC_SDRAMCR1 = 0x00772A22u;
    SEMC_SDRAMCR2 = 0x00010A0Du;
    SEMC_SDRAMCR3 = 0x50210A08u;

    /* init 동안 auto refresh off — 초기화 시퀀스 중에는 컨트롤러가 임의로 refresh 하면 안 됨 */
    SEMC_SDRAMCR3 &= ~SEMC_SDRAMCR3_REN;

    /* init 때 refresh burst = 1 */
    SEMC_SDRAMCR3 &= ~SEMC_SDRAMCR3_REBL_MASK;

    /* power-up settle — JEDEC 규격상 전원 인가 후 100~200us 안정화 대기 필요 */
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

    /* MRS: SDRAM 칩에 동작 모드(BL=8, CL=3) 기록. 모드 값은 IPTXDAT 로 전달된다. */
    if (!SDRAM_SendCommand(SEMC_IPCMD_SDRAM_MRS, SDRAM_BASE_ADDR, SDRAM_MR_BL8_CL3)) {
        UART1_SendString(" -> FAIL: MODE REGISTER SET\r\n");
        return 0;
    }

    /* 초기화 끝 — 이제부터 컨트롤러가 주기적으로 자동 refresh 하도록 REN 켜기 */
    SEMC_SDRAMCR3 |= SEMC_SDRAMCR3_REN;

    semc_delay(2000u);   /* 안정화 대기 후 정상 사용 가능 */
    return 1;
}

/* =========================
 * Step 3-1: 16-bit test
 * ========================= */
/* 16비트 폭으로 SDRAM 에 쓰고 되읽어 데이터가 보존되는지 검증.
 * 참고: 현재 1024워드만 검사 — 빠른 동작 확인용. 전체 32MB 검증이나
 *       refresh(보존성) 검증을 하려면 범위 확대 + 지연 후 재검사가 필요하다. */
int SDRAM_Test16(void)
{
    volatile uint16_t *sdram16 = (volatile uint16_t *)SDRAM_BASE_ADDR;
    const uint32_t test_words = 1024u;

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString("[3] SDRAM 16-bit Integrity Test\r\n");
    UART1_SendString("=================================\r\n");

    /* 쓰기 단계: 각 칸에 자기 인덱스 값을 기록 (주소-데이터 연관 패턴) */
    for (uint32_t i = 0; i < test_words; i++) {
        sdram16[i] = (uint16_t)i;
    }

    /* 검증 단계: 쓴 값이 그대로 읽히는지 확인. 불일치 시 즉시 실패 보고 */
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
/* 32비트 폭으로 SDRAM 에 쓰고 되읽어 검증.
 * 16비트 테스트와 동일한 방식이되, 한 번에 32비트 버스 폭 전체로 접근한다. */
int SDRAM_Test32(void)
{
    volatile uint32_t *sdram32 = (volatile uint32_t *)SDRAM_BASE_ADDR;
    const uint32_t test_words = 1024u;

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString("[4] SDRAM 32-bit Integrity Test\r\n");
    UART1_SendString("=================================\r\n");

    /* 쓰기 단계: 각 칸에 자기 인덱스 값을 기록 */
    for (uint32_t i = 0; i < test_words; i++) {
        sdram32[i] = i;
    }

    /* 검증 단계: 쓴 값이 그대로 읽히는지 확인 */
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
