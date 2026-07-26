#include "sha256.h"

/*
 * SHA-256 (FIPS 180-4) — 베어메탈 직접 구현.
 * Stage 1 의 verify 체인이 Stage 2 이미지 무결성 검증에 사용.
 */

/* 초기 해시값 H0..H7 — 첫 8개 소수(2,3,5,7,11,13,17,19)의 √ 소수부 */
static const uint32_t H_init[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

/* 라운드 상수 K[0..63] — 첫 64개 소수의 ∛(세제곱근) 소수부 */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/* Bit operation helpers */

/* 32비트 오른쪽 회전(rotate). SHA-256 의 기본 확산 도구.
 * n 은 항상 1~31 이므로 (32-n) 이 0/32 가 될 일 없음 → UB 없음. */
static inline uint32_t rotr(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}

/* Ch(choose): x 가 1인 비트는 y, 0인 비트는 z 를 고른다 */
static inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (~x & z);
}

/* Maj(majority): 비트별 다수결 (셋 중 2개 이상이 1이면 1) */
static inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

/* 대문자 시그마 Σ0, Σ1 — 압축 라운드에서 '상태 워드'를 휘젓는 확산 함수 */
static inline uint32_t BSIG0(uint32_t x)
{
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}
static inline uint32_t BSIG1(uint32_t x)
{
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

/* 소문자 시그마 σ0, σ1 — 메시지 스케줄 확장(W[16..63])에 쓰는 확산 함수 */
static inline uint32_t SSIG0(uint32_t x)
{
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}
static inline uint32_t SSIG1(uint32_t x)
{
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

/*
 * 압축 함수 — 512비트 블록 1개를 흡수해 state[8] 을 갱신한다.
 * SHA-256 의 심장. (FIPS 180-4 §6.2.2)
 *
 * 입력 둘:  state[8] (256비트 누적 상태) , block[64] (512비트 메시지 블록)
 * 출력  :  state[8] 가 갱신됨
 */
static void SHA256_Compress(uint32_t state[8], const uint8_t block[SHA256_BLOCK_SIZE])
{
    uint32_t W[64];                  /* 메시지 스케줄 (지역변수 — ctx 에 없음) */
    uint32_t a, b, c, d, e, f, g, h; /* 작업 변수   (지역변수 — ctx 에 없음) */
    uint32_t t1, t2;
    int t;

    /* 1. 메시지 스케줄 */
    /* W[0...15] : 블록을 big-endian 32비트 워드로 읽는다.
       ARM은 little-endian이므로 (uint32_t*) 캐스팅 금지 - byte로 조립*/

    for (t = 0; t < 16; ++t)
    {
        W[t] = ((uint32_t)block[t * 4] << 24 | (uint32_t)block[t * 4 + 1] << 16 | (uint32_t)block[t * 4 + 2] << 8 | (uint32_t)block[t * 4 + 3]);
    }

    /* W[16...63] : 메시지 스케줄 확장(16->64) */
    for (t = 16; t < 64; ++t)
    {
        W[t] = SSIG1(W[t - 2]) + W[t - 7] + SSIG0(W[t - 15]) + W[t - 16];
    }

    /*2. 작업변수 : 현재 상태를 복사*/
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    /* 3. 64 라운드 mixing*/
    for (t = 0; t < 64; ++t)
    {
        t1 = h + BSIG1(e) + Ch(e, f, g) + K[t] + W[t];
        t2 = BSIG0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* 4. 상태 갱신 */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256_Init(sha256_ctx *ctx)
{
    for (int i = 0; i < 8; ++i)
    {
        ctx->state[i] = H_init[i];
    }
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

/* 데이터를 buffer에 추가한다, 64바이트가 차면 압축 함수 호출 */
void SHA256_Update(sha256_ctx *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        ctx->buffer[ctx->buflen++] = data[i];
        ctx->bitlen += 8; /* 추가된 비트수 누적 */
        if (ctx->buflen == SHA256_BLOCK_SIZE)
        {
            SHA256_Compress(ctx->state, ctx->buffer);
            ctx->buflen = 0; /* 버퍼를 비우고 다음 블록 처리 */
        }
    }
}

/* 패딩 후 마지막 블록을 처리, digest 추출 */
void SHA256_Final(sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE])
{
    uint64_t total_bits = ctx->bitlen;
    uint32_t i = ctx->buflen;

    /* 1. 끝 표시에 0x80 추가 */
    ctx->buffer[i++] = 0x80u;

    /* 2. 길이 8바이트 자리가 부족하면(56초과), 이 블록을 0으로 채워 먼저 처리 */
    if (i > 56)
    {
        while (i < 64)
        {
            ctx->buffer[i++] = 0x00u; /* 0x80 뒤는 0x00으로 채운다 */
        }
        SHA256_Compress(ctx->state, ctx->buffer);
        i = 0; /* 다음 블록의 시작점 */
    }

    /* 3. 56바이트까지 0으로 채움 */
    while (i < 56)
    {
        ctx->buffer[i++] = 0x00u;
    }

    /* 4. 마지막 8바이트 = 총 비트 길이(big-endian, MSB 먼저) */
    for (uint32_t j = 0; j < 8; ++j)
    {
        ctx->buffer[56 + j] = (uint8_t)(total_bits >> (56 - 8 * j)); /* 64비트 길이를 8바이트로 나눠 big-endian으로 저장 */
    }

    /* 5.패딩이 완성된 마지막 블록을 처리 */
    SHA256_Compress(ctx->state, ctx->buffer);

    /* 6. 최종 해시값 추출 (state[8]을 big-endian으로 digest(out[32], 32바이트)에 저장) */
    for (i = 0; i < 8; ++i)
    {
        out[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void SHA256_Compute(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE])
{
    sha256_ctx ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(&ctx, out);
}
