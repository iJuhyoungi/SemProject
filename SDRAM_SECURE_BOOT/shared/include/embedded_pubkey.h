#ifndef EMBEDDED_PUBKEY_H
#define EMBEDDED_PUBKEY_H

#include <stdint.h>
#include "bignum.h"

/* RSA-2048 ROOT public modulus (e=65537), embedded in Stage 1.
 *
 * 신뢰 사슬의 기점입니다. 이 값이 바뀌면 기기가 신뢰하는 대상이 통째로
 * 바뀝니다. 실제 제품에서는 Stage 1 이 HAB/eFuse 로 고정되므로 사실상
 * 변경 불가능한 값입니다.
 *
 * Source: /home/juhyoung/.secure_boot_keys/root_private.pem (private key 는 repo 밖에 있습니다)
 * 자동 생성: tools/extract_pubkey.py — 직접 수정하지 마세요
 */
static const bn_t EMBEDDED_ROOT_MODULUS = {
    0xB9163297u,
    0xCECC4403u,
    0xA133A9ABu,
    0xC071044Du,
    0x501B91A0u,
    0xDACFD9F9u,
    0x89955288u,
    0x62AD61ECu,
    0xA26E77AAu,
    0xAA2F0EA3u,
    0x63D837EDu,
    0x96C9C5CAu,
    0xC0355BC7u,
    0xC1D34CBFu,
    0x70731D48u,
    0xE110BC05u,
    0x25B30081u,
    0x259A69CFu,
    0xC53CD04Bu,
    0xE5B8BA26u,
    0x2231C75Bu,
    0x3B7B8C65u,
    0x4FDD4B43u,
    0x9CC46418u,
    0xE4A7D891u,
    0xE8C37BEEu,
    0x02AB2E05u,
    0xC3A2A0B4u,
    0x47D27275u,
    0x8428595Fu,
    0x2942A7BCu,
    0x1B518A59u,
    0xB800D132u,
    0x6BF3AC75u,
    0x1426652Du,
    0xB7AC3BB4u,
    0x83A23741u,
    0x2CC1B7DCu,
    0x683FFCE9u,
    0xDB7B6109u,
    0xA5A121F0u,
    0x7CE69CE6u,
    0x0C6A4105u,
    0x0AA74FD2u,
    0xDA076FF0u,
    0x907DD730u,
    0x1AED69FEu,
    0x0B8E1868u,
    0x255FEA77u,
    0xAD79A30Du,
    0x96CB3006u,
    0xF01C48FFu,
    0x253C8F02u,
    0x76C808C3u,
    0xD9E39685u,
    0xC5EFCFB0u,
    0x4B6D21C7u,
    0xD9E86698u,
    0xA73ABD79u,
    0x9B74B6A8u,
    0x70C36C49u,
    0xD3FE3463u,
    0x4478A5B8u,
    0x8DAE833Cu
};

#endif /* EMBEDDED_PUBKEY_H */
