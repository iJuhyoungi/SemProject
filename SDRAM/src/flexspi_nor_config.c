/*
 * flexspi_nor_config.c
 * -----------------------------------------------------------------------------
 * FCB(Flash Configuration Block) 정의.
 *
 * 전원 ON 직후 Boot ROM 이 가장 먼저(0x60000000 에서) 읽는 512바이트 설정표.
 * "외부 QSPI NOR Flash 를 어떤 핀/속도/명령어로 읽어야 하는가"를 담는다.
 * 이 블록이 잘못되면 Boot ROM 이 Flash 를 못 읽어 부팅 자체가 실패한다.
 * -----------------------------------------------------------------------------
 */
#include <stdint.h>

/* LUT(Look-Up Table)에 들어갈 명령어 종류 코드 (NXP 규격). */
#define CMD_SDR         0x01    /* SDR 모드 커맨드 바이트 전송 */
#define CMD_RADDR_SDR   0x02    /* SDR 모드 읽기 주소 전송 */
#define CMD_DUMMY_SDR   0x0C    /* SDR 모드 더미 사이클 - 0x03에서 0x0C로 수정! */
#define CMD_READ_SDR    0x09    /* SDR 모드 데이터 읽기 - 0x04에서 0x09로 수정! */
#define PAD_1           0x00    /* 1개 데이터 라인 사용 (Single) */
#define PAD_4           0x02    /* 4개 데이터 라인 사용 (Quad) */

/* LUT 한 칸(32비트)을 만드는 매크로. 한 칸에 명령어 2개(op0, op1)가 들어간다.
 * 비트 배치: [op0:8 | pad0:2 | cmd0:6 | op1:8 | pad1:2 | cmd1:6] */
#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1) \
    ( (op0) | ((pad0) << 8) | ((cmd0) << 10) | ((op1) << 16) | ((pad1) << 24) | ((cmd1) << 26) )

/* NXP Boot ROM 규격에 완벽하게 일치하는 512바이트 구조체.
 * 필드 순서/크기는 Boot ROM 이 오프셋으로 직접 읽으므로 절대 변경 금지. */
typedef struct _flexspi_nor_config_ {
    uint32_t tag;                  /* 0x42464346 ("FCFB") — FCB 식별 태그 */
    uint32_t version;              /* 0x56010000 (V1.0) */
    uint32_t reserved0;
    uint8_t  readSampleClkSrc;     /* 1: Internal loopback from DQS pad */
    uint8_t  csHoldTime;           /* 3 — CS 유지 시간 */
    uint8_t  csSetupTime;          /* 3 — CS 셋업 시간 */
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
    uint32_t sflashA1Size;         /* 0x00800000 (8MB) — A1 포트 Flash 용량 */
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
    uint32_t lookupTable[64];      /* LUT — Flash 명령 시퀀스 테이블 */
    uint32_t lutCustomSeq[12];
    uint32_t reserved4[4];

    /* 구조체를 정확히 512바이트로 맞추기 위한 확장 영역 */
    uint32_t pageSize;             /* 256 — 프로그램 페이지 크기 */
    uint32_t sectorSize;           /* 4096 — 이레이즈 섹터 크기 */
    uint8_t  ipcmdSerialClkFreq;   /* 1 */
    uint8_t  isUniformBlockSize;   /* 1 */
    uint8_t  reserved5[2];
    uint8_t  serialNorType;        /* 1 */
    uint8_t  needExitNoCmdMode;
    uint8_t  halfClkForNonReadCmd;
    uint8_t  needRestoreNoCmdMode;
    uint32_t blockSize;            /* 65536 — 블록 이레이즈 크기 */
    uint32_t reserved6[11];
} flexspi_nor_config_t;

/* .flash_config 섹션에 배치 — 링커가 0x60000000 에 매핑한다.
 * used 속성으로 링커가 버리지 않도록 강제 보존. */
__attribute__((section(".flash_config"), used))
const flexspi_nor_config_t qspi_flash_config = {
    .tag = 0x42464346,             /* "FCFB" */
    .version = 0x56010000,
    .readSampleClkSrc = 1,         /* DQS 패드 internal loopback */
    .csHoldTime = 3,
    .csSetupTime = 3,
    .deviceType = 1,               /* Serial NOR */
    .sflashPadType = 1,            /* Pad 타입 */
    .serialClkFreq = 1,            /* 보수적인 저속 클럭으로 시작 (안정성 우선) */
    .sflashA1Size = 0x00800000,    /* 8MB */
    .lookupTable = {
        /* LUT[0]: 읽기 명령 시작 — 0x03(READ) 커맨드 + 24비트 주소 전송 */
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x03, CMD_RADDR_SDR, PAD_1, 0x18),
        /* LUT[1]: 4바이트 데이터 읽기 */
        FLEXSPI_LUT_SEQ(CMD_READ_SDR, PAD_1, 0x04, 0, 0, 0),
    },
    .pageSize = 256,
    .sectorSize = 4096,
    .ipcmdSerialClkFreq = 1,
    .isUniformBlockSize = 1,
    .serialNorType = 1,
    .blockSize = 65536
};
