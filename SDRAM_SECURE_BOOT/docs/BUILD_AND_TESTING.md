# Build & Testing

## Build & Signing Pipeline

`./build.sh` 한 번이면 빌드 + 서명까지 자동 완료.

```
build.sh
 ├─ 1) python3 tools/extract_pubkey.py       # PEM → shared/include/embedded_pubkey.h
 │       └ RSA modulus 를 C 헤더로 (Stage 1 binary 에 임베드)
 │
 ├─ 2) cmake -B build + cmake --build build  # ARM cross-toolchain
 │       ├ Stage 1 (.elf) — embedded_pubkey.h include
 │       └ Stage 2 (.elf) → POST_BUILD:
 │           ├ objcopy .elf → .bin (raw binary)
 │           └ python3 tools/patch_stage2_header.py *.bin
 │               ├ magic/size/CRC32 박기
 │               ├ SHA-256(code) 계산
 │               ├ private key 로 서명 (PKCS#1 v1.5)
 │               └ image 끝에 signature 256 byte 첨부
 │
 └─ 산출물: signed Stage 1 + signed Stage 2 (.bin)
```

`set -e` 로 어느 단계든 실패 시 즉시 중단 → 검증 안 된 image 가 flash 되는 사고 방지.

## Host Tools

| 도구 | 역할 | 실행 빈도 |
|---|---|---|
| `tools/extract_pubkey.py` | PEM → `embedded_pubkey.h` (public modulus 임베드) | 1회 (키 바뀔 때만) |
| `tools/patch_stage2_header.py` | header + SHA + RSA 서명 첨부 | 매 빌드 (POST_BUILD 자동) |

두 도구는 같은 PEM 의 다른 면 (public modulus vs private signing) 을 다룹니다.

## Flash

```bash
./flash_mcu.sh all       # Stage 1 + Stage 2
./flash_mcu.sh stage1    # Stage 1 만
./flash_mcu.sh stage2    # Stage 2 만
```

Stage 1: `0x60002000`, Stage 2: `0x60008000` 으로 flash.

## Key Management

| | 위치 | git | 비고 |
|---|---|---|---|
| Private key | `tests/vectors/rsa_test_key.pem` | ❌ `.gitignore` | 서명 권한, 외부 유출 금지 |
| Public modulus | `shared/include/embedded_pubkey.h` | ✅ tracked | 자동 생성, Stage 1 임베드 |

> 이 프로젝트의 PEM 은 **테스트 전용 키**. production 사용 금지.

---

## Testing

### Host Unit Tests (Unity)

```bash
cd tests
./build.sh    # host x86_64 gcc (RT toolchain 미사용)
./test.sh     # ctest -V
```

| Suite | Tests | Coverage |
|---|---|---|
| `sha256` | 6 | NIST FIPS 180-4 vectors (empty/abc/56-byte) + Streaming API equivalence |
| `bignum` | 26 | add (3) + sub (2) + cmp (3) + bytes (2) + mul (6) + mod (5) + modexp (5) |
| `rsa` | 5 | valid (3) + tampered signature + mismatched hash |
| **Total** | **37** | **ALL PASS @ ~0.07s** |

Reference values:
- Bignum/modexp: Python `int` (임의 정밀도) 로 생성 (`tests/vectors/gen_*.py`)
- RSA: Python `cryptography` 로 keypair + signature 생성

호스트 UT 의 가치 — 보드 cycle (build→flash→reset→UART, 분 단위) 을 `ctest` (초 단위) 로 대체.
알고리즘 정확성을 host 에서 빠르게 검증한 뒤 보드는 smoke 만 확인.

### Board Smoke Test

부팅 시 핵심 함수가 보드 환경에서 호출 가능한지 확인 (host UT 가 전체 검증 담당):

```
[Selftest] SHA-256("abc")        : PASS
[Selftest] bn_add(1+2)           : PASS
[Selftest] bn_mul(1*1)           : PASS
[Selftest] bn_mod(5 mod 3)       : PASS
[Selftest] bn_modexp(2^10%1000)  : PASS
```

---

## Verification Results

### 정상 부팅

```
=============================
[BL1] Stage 1 immutable (Secure Boot)
=============================
[Selftest] running smoke...
[Selftest] SHA-256("abc")        : PASS
[Selftest] bn_add(1+2)           : PASS
[Selftest] bn_mul(1*1)           : PASS
[Selftest] bn_mod(5 mod 3)       : PASS
[Selftest] bn_modexp(2^10%1000)  : PASS
[Selftest] done.

[BL1] Verifying Stage 2 ...
[Verify] SHA-256: 85a150613f39eb7c486484b922880701afffabc55fe8556c0422f6e3845db5e1
[Verify] RSA signature OK
[BL1] Stage 2 OK - jumping to 0x60008000

-----------------------------
[BL2] Stage 2 verified & running
-----------------------------
```

### Tamper Detection — Signature 1-bit Flip

signature 의 단 1비트만 변조 (CRC, SHA 는 통과):

```
[Verify] SHA-256: 85a150613f39eb7c...   ← CRC, SHA 는 PASS (signature 영역 외부)
[Verify] RSA signature FAIL             ← 1비트 변조 검출
[BL1] Stage 2 INVALID - halting (no recovery)
```

RSA modexp 의 **avalanche effect**: signature 1비트 변경 → recovered EM 전체가 random 화
→ PKCS#1 padding 의 magic bytes (`0x00 0x01`) 부터 깨짐 → FAIL.

→ 공격자가 signature 256 byte 중 어느 비트라도 추측해서 맞출 확률 ≈ 0 (RSA-2048 의 안전성).

### Tamper Detection — Code Modification

code 영역의 임의 바이트 변조 시:

```
[Verify] CRC32 FAIL                     ← code 변조는 CRC 단계에서 먼저 검출
```

(CRC 를 우연히 맞춰도 SHA 에서 검출, SHA 도 맞춰도 RSA 에서 검출 — 다층 방어)

---

## CI 통합 가능성

현재 `./build.sh` + `cd tests && ./test.sh` 로 빌드·서명·테스트가 모두 명령 한 줄.
GitHub Actions 등 CI 에 그대로 통합 가능:
- host UT (`ctest`) → 알고리즘 회귀 자동 검증
- 보드 검증은 hardware-in-the-loop 필요 (별도 러너)

→ 알고리즘 구현은 [Crypto Implementation](CRYPTO_IMPLEMENTATION.md) 참조.
