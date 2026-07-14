#include <stdint.h>

/*
 * IVT (Image Vector Table) + Boot Data.
 *
 * RT1020 boot ROM 이 0x60001000 (IVT) 를 읽어 entry point 와 boot_data 를
 * 찾는다. Secure Boot 에서도 이 책임은 Stage 1 에 있다 — Stage 2 binary 에
 * IVT 가 또 있으면 dead data 가 되므로 Stage 2 linker 는 .ivt 를 DISCARD.
 *
 * OTA_AB 에서 verbatim 이식 (FCB/IVT 주소가 안 바뀌므로 수정 없음).
 */

#define IVT_MAJOR_VERSION           0x40
#define IVT_MAJOR_VERSION_SHIFT     24
#define IVT_HEADER_TAG              0xD1
#define IVT_HEADER_TAG_SHIFT        0
#define IVT_HEADER_LENGTH           0x20
#define IVT_HEADER_LENGTH_SHIFT     16

#define IVT_HEADER ( \
    (IVT_MAJOR_VERSION << IVT_MAJOR_VERSION_SHIFT) | \
    (IVT_HEADER_TAG << IVT_HEADER_TAG_SHIFT) | \
    (IVT_HEADER_LENGTH << IVT_HEADER_LENGTH_SHIFT) \
)

extern void Reset_Handler(void);

typedef struct _ivt_ {
    uint32_t hdr;
    uint32_t entry;
    uint32_t reserved1;
    uint32_t dcd;
    uint32_t boot_data;
    uint32_t self;
    uint32_t csf;
    uint32_t reserved2;
} ivt_t;

typedef struct _boot_data {
    uint32_t start;
    uint32_t size;
    uint32_t plugin;
    uint32_t placeholder;
} boot_data_t;

#define FLASH_BASE      0x60000000
#define IVT_BASE        0x60001000
#define BOOT_DATA_BASE  0x60001020

__attribute__((section(".ivt"), used))
const ivt_t image_vector_table = {
    .hdr = IVT_HEADER,
    .entry = (uint32_t)Reset_Handler,
    .reserved1 = 0,
    .dcd = 0,
    .boot_data = BOOT_DATA_BASE,
    .self = IVT_BASE,
    .csf = 0,
    .reserved2 = 0
};


__attribute__((section(".boot_data"), used))
const boot_data_t boot_data = {
    .start = FLASH_BASE,
    .size = 0x00800000, // 8MB
    .plugin = 0,
    .placeholder = 0xFFFFFFFF
};
