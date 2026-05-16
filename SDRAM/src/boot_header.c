/*
 * boot_header.c
 * -----------------------------------------------------------------------------
 * i.MX RT1020 의 부팅 관문인 IVT(Image Vector Table) 와 Boot Data 를 정의한다.
 *
 * [부팅 흐름] 전원 ON -> Boot ROM 실행 -> FCB(0x60000000) 읽어 외부 Flash 인식
 *            -> IVT(0x60001000) 읽어 "펌웨어 진입점"과 "복사할 영역" 파악
 *            -> Reset_Handler 로 점프
 *
 * 이 파일의 두 구조체(image_vector_table, boot_data)는 링커 스크립트가 지정한
 * 고정 주소에 배치되어, Boot ROM 이 그대로 찾아 읽어간다.
 * -----------------------------------------------------------------------------
 */
#include <stdint.h>

/* IVT 헤더(hdr) 필드 값과 비트 위치.
 * hdr 은 [tag:8 | length:16 | version:8] 형태로 조립된다. */
#define IVT_MAJOR_VERSION       0x40    /* IVT 포맷 버전 (HAB v4) */
#define IVT_MAJOR_VERSION_SHIFT 24      /* version 필드는 상위 8비트(31:24) */
#define IVT_HEADER_TAG          0xD1    /* IVT 임을 식별하는 고정 태그 */
#define IVT_HEADER_TAG_SHIFT    0       /* tag 필드는 하위 8비트(7:0) */
#define IVT_HEADER_LENGTH       0x20    /* IVT 구조체 크기 = 32바이트 */
#define IVT_HEADER_LENGTH_SHIFT 16      /* length 필드는 비트(23:16) — 8에서 16으로 수정! */

/* 세 값을 OR 로 합쳐 최종 hdr 워드(= 0x402000D1)를 만든다. */
#define IVT_HEADER ( \
    (IVT_MAJOR_VERSION << IVT_MAJOR_VERSION_SHIFT) | \
    (IVT_HEADER_LENGTH << IVT_HEADER_LENGTH_SHIFT) | \
    (IVT_HEADER_TAG << IVT_HEADER_TAG_SHIFT) \
)

/* 스타트업 코드의 진입점. 실제 정의는 startup 파일에 있다. */
extern void Reset_Handler(void);

/* IVT 구조체 — Boot ROM 이 기대하는 32바이트 고정 레이아웃 (순서/크기 변경 금지). */
typedef struct _ivt_ {
    uint32_t hdr;        /* 위에서 만든 헤더 워드 */
    uint32_t entry;      /* 펌웨어 진입 주소 (Reset_Handler) */
    uint32_t reserved1;  /* 예약 — 0 */
    uint32_t dcd;        /* Device Config Data 포인터 (미사용 시 0) */
    uint32_t boot_data;  /* 아래 boot_data 구조체의 주소 */
    uint32_t self;       /* IVT 자기 자신의 주소 (Boot ROM 검증용) */
    uint32_t csf;        /* Command Sequence File 포인터 (비보안 부팅 시 0) */
    uint32_t reserved2;  /* 예약 — 0 */
} ivt_t;

/* Boot Data 구조체 — "어디서부터 얼마만큼이 부팅 이미지인가"를 Boot ROM 에 알려준다. */
typedef struct _boot_data_ {
    uint32_t start;       /* 이미지 시작 주소 */
    uint32_t size;        /* 이미지 전체 크기 */
    uint32_t plugin;      /* 플러그인 부팅 여부 (0 = 일반 부팅) */
    uint32_t placeholder; /* 정렬/예약용 패딩 */
} boot_data_t;

/* Boot ROM 이 약속된 고정 주소에서 찾도록 정해진 위치 매크로. */
#define FLASH_BASE           0x60000000  /* 외부 QSPI NOR Flash 의 시작 주소 */
#define IVT_BASE             0x60001000  /* IVT 가 놓일 주소 */
#define BOOT_DATA_BASE       0x60001020  /* Boot Data 가 놓일 주소 (IVT 바로 뒤, 0x20 오프셋) */

/* .ivt 섹션에 배치 — 링커 스크립트가 이 섹션을 0x60001000 에 매핑한다.
 * used 속성: 코드에서 직접 참조하지 않아도 링커가 버리지 않도록 강제 보존. */
__attribute__((section(".ivt"), used))
const ivt_t image_vector_table = {
    .hdr = IVT_HEADER,
    .entry = (uint32_t)Reset_Handler,   /* 부팅 후 점프할 주소 */
    .reserved1 = 0,
    .dcd = 0,                           /* DCD 미사용 */
    .boot_data = BOOT_DATA_BASE,        /* Boot Data 위치 알려줌 */
    .self = IVT_BASE,                   /* 이 IVT 자신의 주소 */
    .csf = 0,                           /* 비보안(평문) 부팅 */
    .reserved2 = 0
};

/* .boot_data 섹션에 배치 — 링커 스크립트가 0x60001020 에 매핑한다. */
__attribute__((section(".boot_data"), used))
const boot_data_t boot_data = {
    .start = FLASH_BASE,
    .size = 0x00800000,        /* 8MB Flash 기준 — 전체 Flash 를 이미지 영역으로 잡음 */
    .plugin = 0,               /* 일반 부팅 */
    .placeholder = 0xFFFFFFFF
};
