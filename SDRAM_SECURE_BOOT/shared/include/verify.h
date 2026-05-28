#ifndef VERIFY_H
#define VERIFY_H

#include <stdint.h>
#include "bignum.h"

#define IMG_MAGIC_OFFSET    0x1Cu
#define IMG_SIZE_OFFSET     0x20u
#define IMG_CRC_OFFSET      0x24u
#define IMG_MAGIC_VALUE     0xDEADBEEF
#define IMG_SIZE_MIN        0x100u
#define IMG_SIZE_MAX        0x40000u

/**
 * image 검증 : vector sanity -> magic -> size -> CRC32 -> SHA256 -> RSA-2048
 * 모든 단계 통과 시 1, 하나라도 실패 시 0, 단계별 "[Verify] ..." UART 출력
 */

int verify_image(uint32_t base, const bn_t module);
void jump_to_image(uint32_t addr);

#endif /* VERIFY_H */