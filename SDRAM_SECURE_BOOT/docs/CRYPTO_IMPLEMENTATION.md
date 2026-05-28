# Crypto Implementation

외부 라이브러리 없이 직접 구현한 crypto stack. 모두 `shared/` 에 위치.

| 모듈 | 파일 | 책임 |
|---|---|---|
| SHA-256 | `sha256.{h,c}` | image fingerprint |
| Bignum | `bignum.{h,c}` | 2048-bit 산술 (add/sub/cmp/mul/mod/modexp) |
| RSA verify | `rsa.{h,c}` | PKCS#1 v1.5 signature 검증 |
| Minimal libc | `string_min.c` | memset/memcpy/memcmp (베어메탈) |

---

## 1. SHA-256 (FIPS 180-4)

표준 SHA-256 의 전체 직접 구현.

- **State**: 8 × 32-bit (`H[0..7]`)
- **Message schedule**: 64 × 32-bit (`W[0..63]`)
- **Compression**: 64 rounds (Ch, Maj, Σ0, Σ1, σ0, σ1 + round constant K[64])
- **Padding**: `1` bit + `0` padding + 64-bit message length (big-endian)
- **Byte assembly**: little-endian ARM 위에서 big-endian word 조립

### API

```c
void SHA256_Init(SHA256_Context *ctx);
void SHA256_Update(SHA256_Context *ctx, const uint8_t *data, size_t len);
void SHA256_Final(SHA256_Context *ctx, uint8_t digest[32]);
void SHA256_Compute(const uint8_t *data, size_t len, uint8_t digest[32]);  // 한 번에
```

### 베어메탈 함정 — memset 자동 치환

GCC 의 `-ftree-loop-distribute-patterns` 최적화는 0-fill 루프를 `memset()` 호출로 치환합니다.
베어메탈엔 libc 가 없어 `undefined reference to memset` 링크 에러 발생.

해결:
- `string_min.c` 에 `memset`/`memcpy`/`memcmp` 직접 구현
- 단, 그 파일 자신은 `-fno-tree-loop-distribute-patterns -fno-builtin` 으로 컴파일 (자기 자신이 memset 호출로 치환되는 무한 재귀 방지)

---

## 2. Bignum — 2048-bit Arithmetic

### 표현

```c
#define BN_BITS   2048
#define BN_WORDS  64                 // 2048 / 32
#define BN_BYTES  256                // 2048 / 8
typedef uint32_t bn_t[BN_WORDS];     // 2048-bit 정수
typedef uint32_t bn2_t[128];         // 4096-bit (mul 결과 임시)
```

- **Little-endian word ordering**: `v[0]` = LSW, `v[63]` = MSW
  → carry 가 `v[0] → v[1] → ...` 방향으로 자연스럽게 전파
- **워드 = 32-bit**: Cortex-M7 native register 폭 (UMULL, UMLAL 활용)

### API

```c
void     bn_zero(bn_t r);
void     bn_copy(bn_t r, const bn_t a);
int      bn_is_zero(const bn_t a);
int      bn_cmp(const bn_t a, const bn_t b);            // -1 / 0 / +1
uint32_t bn_add(bn_t r, const bn_t a, const bn_t b);    // 반환: carry
uint32_t bn_sub(bn_t r, const bn_t a, const bn_t b);    // 반환: borrow
void     bn_mul(bn2_t prod, const bn_t a, const bn_t b);
void     bn_mod(bn_t r, const bn2_t a, const bn_t n);
void     bn_modexp(bn_t r, const bn_t m, uint32_t e, const bn_t n);
void     bn_from_bytes_be(bn_t r, const uint8_t bytes[256]);
void     bn_to_bytes_be(uint8_t bytes[256], const bn_t a);
```

### Carry / Borrow 패턴 (uint64 누산)

```c
// add
uint64_t sum = (uint64_t)a[i] + b[i] + carry;
r[i] = (uint32_t)sum;
carry = (uint32_t)(sum >> 32);

// sub
uint64_t diff = (uint64_t)a[i] - b[i] - borrow;
r[i] = (uint32_t)diff;
borrow = (uint32_t)(diff >> 63);   // 음수면 부호 확장으로 비트63=1
```

### Multiplication (Schoolbook)

```c
void bn_mul(bn2_t prod, const bn_t a, const bn_t b) {
    bn2_zero(prod);
    for (uint32_t i = 0; i < BN_WORDS; ++i) {
        uint32_t carry = 0;
        for (uint32_t j = 0; j < BN_WORDS; ++j) {
            uint64_t t = (uint64_t)a[i] * b[j] + prod[i + j] + carry;
            prod[i + j] = (uint32_t)t;
            carry = (uint32_t)(t >> 32);
        }
        prod[i + BN_WORDS] = carry;
    }
}
```

한 자리 누산식의 최댓값이 정확히 `2^64 - 1` 이라 uint64 누산이 overflow 없이 안전.
(자세한 유도는 학습 노트 참조)

### Byte ↔ Word 변환

RSA 표준 (PKCS#1) 은 모든 큰 수를 **big-endian byte** 로 표현. 내부는 **little-endian word**.
두 reversal 동시 발생:
1. word 순서 반전 (BE → LE word)
2. 각 워드 내 4바이트는 BE 유지

```
v[0]  (LSW) = bytes[252..255]   // BE byte 의 끝
v[63] (MSW) = bytes[0..3]       // BE byte 의 시작
```

---

## 3. Modular Exponentiation

`signature^65537 mod n` 의 핵심. **square-and-multiply** (binary method, MSB→LSB).

```c
void bn_modexp(bn_t r, const bn_t m, uint32_t e, const bn_t n) {
    bn_zero(r); r[0] = 1;                  // r = 1
    if (e == 0) return;

    int top_bit = 31;
    while (top_bit >= 0 && !((e >> top_bit) & 1u)) --top_bit;

    bn2_t tmp;
    for (int i = top_bit; i >= 0; --i) {
        bn_mul(tmp, r, r); bn_mod(r, tmp, n);              // square
        if ((e >> i) & 1u) { bn_mul(tmp, r, m); bn_mod(r, tmp, n); }  // multiply
    }
}
```

- `e = 65537` = `0x10001` = `2^16 + 1` (페르마 소수 F4, RSA 산업 표준 public exponent)
- 비트가 1인 자리 2개 → 약 16 square + 1 multiply + 17 mod
- 보드 약 1.4초 (Cortex-M7 600 MHz, 부팅 1회뿐)

### `bn_mod` — Bit-by-Bit Long Division

```c
void bn_mod(bn_t r, const bn2_t a, const bn_t n) {
    bn_zero(r);
    uint32_t r_overflow = 0;

    for (int i = (int)(128 * 32) - 1; i >= 0; --i) {
        // r = r << 1 (잘린 최상위 비트를 r_overflow 로 보관)
        uint32_t new_overflow = (r[BN_WORDS - 1] >> 31) & 1u;
        uint32_t carry = 0;
        for (uint32_t k = 0; k < BN_WORDS; ++k) {
            uint32_t next_carry = (r[k] >> 31) & 1u;
            r[k] = (r[k] << 1) | carry;
            carry = next_carry;
        }
        r_overflow = new_overflow;

        // a 의 i번째 비트를 r 의 LSB 로
        if ((a[i >> 5] >> (i & 31)) & 1u) r[0] |= 1u;

        // r >= n 이면 r -= n
        if (r_overflow || bn_cmp(r, n) >= 0) {
            bn_sub(r, r, n);
            r_overflow = 0;
        }
    }
}
```

핵심:
- 초등학교 long division 을 비트 단위로 (= 입력 비트를 하나씩 내려쓰고, n 이상이면 빼기)
- **`r_overflow`**: r 그릇이 2048-bit 고정인데 n < 2^2048 이라 shift 시 최상위 비트가 잘림.
  그 1비트를 별도 보관해 정확한 비교 보장
- Invariant: 매 iteration 시작 시 `r < n` → 조건부 sub 한 번이면 회복

> 알고리즘 선택 근거 (vs Barrett / Montgomery / Knuth): 부팅 1회뿐이라 단순성 우선.
> 성능 필요 시 Barrett reduction 으로 약 7배 가속 가능 (Roadmap 참조).

---

## 4. RSA-2048 PKCS#1 v1.5 Verify

RFC 8017 RSASSA-PKCS1-v1_5 (with SHA-256) 의 검증 측.

```c
int rsa_verify_pkcs1_v15_sha256(
    const uint8_t expected_hash[32],
    const uint8_t signature[256],
    const bn_t modulus);
```

### Encoded Message (EM) 구조

```
┌──────┬──────┬──────────────────┬──────┬──────────────┬───────────┐
│ 0x00 │ 0x01 │ PS = 0xFF × 202  │ 0x00 │ DigestInfo   │ hash[32]  │
└──────┴──────┴──────────────────┴──────┴──────────────┴───────────┘
  [0]   [1]    [2..203]           [204]  [205..223]      [224..255]
```

| 영역 | 역할 |
|---|---|
| `0x00 0x01` | magic (서명용 마커, 암호화는 `0x00 0x02`) |
| PS (`0xFF` × 202) | padding string, 길이 ≥ 8 표준 요구 |
| `0x00` | PS ↔ DigestInfo separator |
| DigestInfo (19 B) | SHA-256 OID 의 DER encoding (정해진 상수) |
| hash (32 B) | 실제 SHA-256 결과 |

### 검증 흐름

```
1. signature^65537 mod n        → recovered EM (256 byte)
2. EM[0..1] == 0x00 0x01 ?
3. EM[2..203] 모두 0xFF ? (PS 검사, 길이 >= 8)
4. EM[204] == 0x00 ?
5. EM[205..223] == SHA256 DigestInfo prefix ?
6. EM[224..255] == expected_hash ?
→ 모두 통과 시 return 1
```

### 왜 `e = 65537` 인가

- `e = 3`: 빠르지만 Coppersmith's attack 등에 취약 (작은 메시지 직접 회복 위험)
- `e = 65537`: 페르마 소수 F4. 알려진 attack 없음. 산업 표준

---

## 5. Key Separation

```
[Host PC (개발자)]                    [Board (사용자)]
private key (PEM)                      public modulus 만 (코드에 박힘)
  ├ 서명 권한 보유                       ├ 검증만 가능
  ├ 절대 git/보드 진입 X                 ├ 펌웨어 덤프돼도 무해
  └ tests/vectors/rsa_test_key.pem      └ shared/include/embedded_pubkey.h
```

- Private key 로 호스트가 서명 → 보드는 public modulus 로 검증만
- 공격자가 보드 펌웨어 덤프해도 public modulus 만 보임 → signature 위조 불가
- 이것이 secure boot 의 본질 (비대칭 키 분리)

→ 빌드/서명 파이프라인은 [Build & Testing](BUILD_AND_TESTING.md) 참조.
