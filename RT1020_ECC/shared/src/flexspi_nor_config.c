// #include "flexspi_nor_config.h"

// // __attribute__((section(".flash_config"), used))
// // const flexspi_nor_config_t qspi_flash_config = {
// //     .tag = 0x42464346,
// //     .version = 0x56010000,
// //     .readSampleClkSrc = 1,
// //     .csHoldTime = 3,
// //     .csSetupTime = 3,
// //     .deviceType = 1,
// //     .sflashPadType = 1,
// //     .serialClkFreq = 1,
// //     .sflashA1Size = 0x00800000,
// //     .lookupTable = {
// //         FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x03, CMD_RADDR_SDR, PAD_1, 0x18),
// //         FLEXSPI_LUT_SEQ(CMD_READ_SDR, PAD_1, 0x04, 0, 0, 0),
// //     },
// //     .pageSize = 256,
// //     .sectorSize = 4096,
// //     .ipcmdSerialClkFreq = 1,
// //     .isUniformBlockSize = 1,
// //     .serialNorType = 1,
// //     .blockSize = 65536
// // };

// __attribute__((section(".flash_config"), used))
// const flexspi_nor_config_t qspi_flash_config = {
//     .tag = 0x42464346,             
//     .version = 0x56010000,
//     .readSampleClkSrc = 1,
//     .csHoldTime = 3,
//     .csSetupTime = 3,
//     .deviceType = 1,               
//     .sflashPadType = 1,            
//     .serialClkFreq = 1,            
//     .sflashA1Size = 0x00800000,    
//     .lookupTable = {
//         FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x03, CMD_RADDR_SDR, PAD_1, 0x18),
//         FLEXSPI_LUT_SEQ(CMD_READ_SDR, PAD_1, 0x04, 0, 0, 0),
//     },
//     .pageSize = 256,
//     .sectorSize = 4096,
//     .ipcmdSerialClkFreq = 1,
//     .isUniformBlockSize = 1,
//     .serialNorType = 1,
//     .blockSize = 65536
// };


#include "flexspi_nor_config.h"

#define RADDR_SDR 0x02
#define DUMMY_SDR 0x0C
#define READ_SDR  0x09
#define WRITE_SDR 0x08
#define STOP      0x00
#define CMD_SDR   0x01


__attribute__((section(".flash_config"), used))
const flexspi_nor_config_t qspi_flash_config = {
    .tag = 0x42464346,            /* FCFB */
    .version = 0x56010100,        /* SDK EVK ROMAPI example */

    .readSampleClkSrc = 1,        /* Loopback from DQS pad */
    .csHoldTime = 3,
    .csSetupTime = 3,

    /*device mode 설정 : ROM INIT이 chip의 QE bit를 set하도록 시킴*/
    .deviceModeCfgEnable=1,             /* device mode cfg 활성 */
    .deviceModeType=1,                  /* 1 = Quad Enable type */
    .waitTimeCfgCommands=1,             /* cfg 명령 후 100us 대기 */
    .deviceModeSeq=0x00000401u,         /* RM Table 9-15: bit[7:0]=seqNum=1, bit[15:8]=seqId=4 */
    .deviceModeArg=0x00000200u,         /* 16-bit write: SR1=0x00, SR2=0x02 (= QE bit 1) */

    /* SafeConfigFreqEnable */
    // .controllerMiscOption = (1u << 4),
    .controllerMiscOption = 0,

    .deviceType = 1,              /* Serial NOR */
    .sflashPadType = 4,           /* Quad */
    .serialClkFreq = 7,           /* 133 MHz */
    .sflashA1Size = 8u * 1024u * 1024u,

    .lookupTable = {
        /* READ: 0xEB, 1-4-4 */
        [0]  = FLEXSPI_LUT_SEQ(CMD_SDR,  PAD_1, 0xEB, RADDR_SDR, PAD_4, 0x18),
        [1]  = FLEXSPI_LUT_SEQ(DUMMY_SDR, PAD_4, 0x06, READ_SDR,  PAD_4, 0x04),

        /* READ STATUS: 0x05 */
        [4 * 1 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x05, READ_SDR, PAD_1, 0x01),

        /* WRITE ENABLE: 0x06 */
        [4 * 3 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x06, STOP, PAD_1, 0x00),

        /* ★ WRITE STATUS REGISTER: 0x01 (SR1+SR2 16-bit write to set QE bit) */                                                                                                         
        [4 * 4 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x01, WRITE_SDR, PAD_1, 0x02),

        /* ERASE SECTOR: 0x20 */
        [4 * 5 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x20, RADDR_SDR, PAD_1, 0x18),

        /* ERASE BLOCK: 0xD8 */
        [4 * 8 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0xD8, RADDR_SDR, PAD_1, 0x18),

        /* PAGE PROGRAM: 0x32, quad data */
        [4 * 9 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x32, RADDR_SDR, PAD_1, 0x18),
        [4 * 9 + 1] = FLEXSPI_LUT_SEQ(WRITE_SDR, PAD_4, 0x04, STOP, PAD_1, 0x00),

        /* CHIP ERASE: 0x60 */
        [4 * 11 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x60, STOP, PAD_1, 0x00),
    },

    .pageSize = 256u,
    .sectorSize = 4u * 1024u,
    .ipcmdSerialClkFreq = 1u,     /* 30 MHz */
    .blockSize = 64u * 1024u,
    .isUniformBlockSize = 0u,

    /*
     * SDK ROMAPI example는 이 뒤 필드들을 명시적으로 세팅하지 않음.
     * 따라서 0으로 두는 편이 EVK 예제와 가장 가깝다.
     */
    .serialNorType = 0u,
    .needExitNoCmdMode = 0u,
    .halfClkForNonReadCmd = 0u,
    .needRestoreNoCmdMode = 0u,
};