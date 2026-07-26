#ifndef FLEXSPI_NOR_CONFIG_H
#define FLEXSPI_NOR_CONFIG_H


#include <stdint.h>

/* NXP 데이터시트에 맞춘 정확한 매크로 번호 */
#define CMD_SDR         0x01
#define CMD_RADDR_SDR   0x02
#define CMD_DUMMY_SDR   0x0C  // 0x03에서 0x0C로 수정!
#define CMD_READ_SDR    0x09  // 0x04에서 0x09로 수정!
#define PAD_1           0x00
#define PAD_4           0x02

#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1) \
    ( (op0) | ((pad0) << 8) | ((cmd0) << 10) | ((op1) << 16) | ((pad1) << 24) | ((cmd1) << 26) )

/* NXP Boot ROM 규격에 완벽하게 일치하는 512바이트 구조체 */
typedef struct _flexspi_nor_config_ {
    uint32_t tag;                  /* 0x42464346 ("FCFB") */
    uint32_t version;              /* 0x56010000 (V1.0) */
    uint32_t reserved0;
    uint8_t  readSampleClkSrc;     /* 1: Internal loopback from DQS pad */
    uint8_t  csHoldTime;           /* 3 */
    uint8_t  csSetupTime;          /* 3 */
    uint8_t  columnAddressWidth;
    uint8_t  deviceModeCfgEnable;
    uint8_t  deviceModeType;
    uint16_t waitTimeCfgCommands;
    uint32_t deviceModeSeq;
    uint32_t deviceModeArg;
    uint8_t  configCmdEnable;
    uint8_t  configModeType[3];
    uint32_t configCmdSeqs[3];
    uint32_t reserved1;
    uint32_t configCmdArgs[3];
    uint32_t reserved2;
    uint32_t controllerMiscOption; 
    uint8_t  deviceType;           /* 1: Serial NOR */
    uint8_t  sflashPadType;        /* 4: Quad Pad */
    uint8_t  serialClkFreq;        /* 1: 30MHz */
    uint8_t  lutCustomSeqEnable;
    uint32_t reserved3[2];
    uint32_t sflashA1Size;         /* 0x00800000 (8MB) */
    uint32_t sflashA2Size;
    uint32_t sflashB1Size;
    uint32_t sflashB2Size;
    uint32_t csPadSettingOverride;
    uint32_t sclkPadSettingOverride;
    uint32_t dataPadSettingOverride;
    uint32_t dqsPadSettingOverride;
    uint32_t timeoutInMs;
    uint32_t commandInterval;
    uint16_t dataValidTime[2];
    uint16_t busyOffset;
    uint16_t busyBitPolarity;
    uint32_t lookupTable[64];
    uint32_t lutCustomSeq[12];
    uint32_t reserved4[4];

    /* 구조체를 정확히 512바이트로 맞추기 위한 확장 영역 */
    uint32_t pageSize;             /* 256 */
    uint32_t sectorSize;           /* 4096 */
    uint8_t  ipcmdSerialClkFreq;   /* 1 */
    uint8_t  isUniformBlockSize;   /* 1 */
    uint8_t  reserved5[2];       
    uint8_t  serialNorType;        /* 1 */
    uint8_t  needExitNoCmdMode;    
    uint8_t  halfClkForNonReadCmd; 
    uint8_t  needRestoreNoCmdMode; 
    uint32_t blockSize;            /* 65536 */
    uint32_t reserved6[11];        
} flexspi_nor_config_t;

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

// #include <stdint.h>

// /* NXP 데이터시트에 맞춘 매크로 */
// #define CMD_SDR         0x01
// #define CMD_RADDR_SDR   0x02
// #define CMD_DUMMY_SDR   0x0C
// #define CMD_READ_SDR    0x09
// #define PAD_1           0x00
// #define PAD_4           0x02

// #define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1) \
//     ((op0) | ((pad0) << 8) | ((cmd0) << 10) | ((op1) << 16) | ((pad1) << 24) | ((cmd1) << 26))

// typedef struct _flexspi_nor_config_ {
//     uint32_t tag;
//     uint32_t version;
//     uint32_t reserved0;
//     uint8_t  readSampleClkSrc;
//     uint8_t  csHoldTime;
//     uint8_t  csSetupTime;
//     uint8_t  columnAddressWidth;
//     uint8_t  deviceModeCfgEnable;
//     uint8_t  deviceModeType;
//     uint16_t waitTimeCfgCommands;
//     uint32_t deviceModeSeq;
//     uint32_t deviceModeArg;
//     uint8_t  configCmdEnable;
//     uint8_t  configModeType[3];
//     uint32_t configCmdSeqs[3];
//     uint32_t reserved1;
//     uint32_t configCmdArgs[3];
//     uint32_t reserved2;
//     uint32_t controllerMiscOption;
//     uint8_t  deviceType;
//     uint8_t  sflashPadType;
//     uint8_t  serialClkFreq;
//     uint8_t  lutCustomSeqEnable;
//     uint32_t reserved3[2];
//     uint32_t sflashA1Size;
//     uint32_t sflashA2Size;
//     uint32_t sflashB1Size;
//     uint32_t sflashB2Size;
//     uint32_t csPadSettingOverride;
//     uint32_t sclkPadSettingOverride;
//     uint32_t dataPadSettingOverride;
//     uint32_t dqsPadSettingOverride;
//     uint32_t timeoutInMs;
//     uint32_t commandInterval;
//     uint16_t dataValidTime[2];
//     uint16_t busyOffset;
//     uint16_t busyBitPolarity;
//     uint32_t lookupTable[64];
//     uint32_t lutCustomSeq[12];
//     uint32_t reserved4[4];

//     uint32_t pageSize;
//     uint32_t sectorSize;
//     uint8_t  ipcmdSerialClkFreq;
//     uint8_t  isUniformBlockSize;
//     uint8_t  reserved5[2];
//     uint8_t  serialNorType;
//     uint8_t  needExitNoCmdMode;
//     uint8_t  halfClkForNonReadCmd;
//     uint8_t  needRestoreNoCmdMode;
//     uint32_t blockSize;
//     uint32_t reserved6[11];
// } flexspi_nor_config_t;

/* 선언만 둠 */
extern const flexspi_nor_config_t qspi_flash_config;

#endif
