#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_DIGEST_SIZE  32u     /* 해시 출력: 256 bit = 32 byte */
#define SHA256_BLOCK_SIZE   64u     /* 처리 단위: 512 bit = 64 byte */

/*
* SHA-256 중간 상태.
* Update 호출 사이에 유지되어야 하므로 구조체로 들고 다닌다.
*/

typedef struct {
    uint32_t state[8];      /* 256비트 해시 상태 (H0..H7 자리) */
    uint64_t bitlen;        /* 지금까지 흡수한 총 비트 수
                               (패딩의 마지막 64비트에 들어감) */

    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint32_t buflen;
}sha256_ctx;

void SHA256_Init(sha256_ctx *ctx);                                                              /* 상태를 초기 해시값으로 리셋 */
void SHA256_Update(sha256_ctx *ctx, const uint8_t *data, size_t len);                           /* data[0..len) 를 흡수. 여러 번 나눠 불러도 된다 */
void SHA256_Final(sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE]);                            /* 패딩 처리 후 최종 digest(32바이트)를 out 에 쓴다 */
void SHA256_Compute(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);          /* Init+Update+Final 을 한 번에 — 연속된 한 덩이 데이터용 편의 함수 */


#endif /* SHA256_H */