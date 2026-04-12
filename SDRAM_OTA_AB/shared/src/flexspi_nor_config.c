#include "flexspi_nor_config.h"

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

__attribute__((section(".flash_config"), used))
const flexspi_nor_config_t qspi_flash_config = {
    .tag = 0x42464346,             
    .version = 0x56010000,
    .readSampleClkSrc = 1,
    .csHoldTime = 3,
    .csSetupTime = 3,
    .deviceType = 1,               
    .sflashPadType = 1,            
    .serialClkFreq = 1,            
    .sflashA1Size = 0x00800000,    
    .lookupTable = {
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x03, CMD_RADDR_SDR, PAD_1, 0x18),
        FLEXSPI_LUT_SEQ(CMD_READ_SDR, PAD_1, 0x04, 0, 0, 0),
    },
    .pageSize = 256,
    .sectorSize = 4096,
    .ipcmdSerialClkFreq = 1,
    .isUniformBlockSize = 1,
    .serialNorType = 1,
    .blockSize = 65536
};

