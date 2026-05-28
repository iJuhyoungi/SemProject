# SDRAM Secure Boot — i.MX RT1020 Bare-Metal Implementation

> 베어메탈 환경에서 SHA-256 + RSA-2048 을 외부 라이브러리 없이 직접 구현해
> 구성한 2-stage secure boot. Cortex-M7 (NXP i.MX RT1020) target.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![crypto](https://img.shields.io/badge/crypto-SHA--256_%2B_RSA--2048-orange)]()
[![tests](https://img.shields.io/badge/host_UT-37%2F37_PASS-green)]()
[![deps](https://img.shields.io/badge/crypto_deps-0-success)]()

---

## What is this?

i.MX RT1020 (Cortex-M7) 의 **2-stage secure boot**. Stage 1 (immutable verifier) 이
Stage 2 (application) 의 **무결성 + 인증** 을 검증한 뒤에만 점프. 검증 실패 시 halt.

핵심 특징:
- **외부 crypto 라이브러리 의존성 0** — SHA-256, bignum, RSA-2048 모두 직접 구현
- **6단계 trust chain** — vector → magic → size → CRC32 → SHA-256 → RSA-2048
- **호스트 서명 파이프라인** — 빌드 시스템 통합 (`./build.sh` 한 번에 signed image)
- **위조 차단 시연** — signature 1비트 변조도 검출 → halt

---

## Quick Start

```bash
./build.sh           # Stage 1 + Stage 2 빌드 + 자동 서명
./flash_mcu.sh all   # 양쪽 flash
```

호스트 UT:
```bash
cd tests && ./build.sh && ./test.sh   # 37/37 PASS
```

---

## Documentation

| 문서 | 내용 |
|---|---|
| [Architecture](docs/ARCHITECTURE.md) | 2-stage 구조, 메모리 맵, boot flow, image layout |
| [Trust Chain](docs/TRUST_CHAIN.md) | verify chain 6단계 — 각 단계의 검증 대상과 의미 |
| [Crypto Implementation](docs/CRYPTO_IMPLEMENTATION.md) | SHA-256, bignum 2048-bit, modexp, PKCS#1 v1.5 |
| [Build & Testing](docs/BUILD_AND_TESTING.md) | 서명 파이프라인, host UT, tamper detection 결과 |

---

## Status

| 항목 | 상태 |
|---|---|
| E2E 보드 검증 | ✅ 정상 부팅 + Stage 2 점프 |
| 위조 차단 시연 | ✅ signature 1비트 변조 → halt |
| Host UT | ✅ 37/37 PASS (sha 6 + bignum 26 + rsa 5) |
| Build automation | ✅ CMake POST_BUILD signing pipeline |

---

## Project Layout

```
SDRAM_SECURE_BOOT/
├── bootloader/{stage1, stage2}/    # immutable verifier + verified payload
├── shared/{include, src}/          # bignum, sha256, rsa, string_min, ...
├── tests/                          # Host UT (Unity) + Python reference vectors
├── tools/                          # extract_pubkey.py, patch_stage2_header.py
├── docs/                           # 상세 문서 (위 표)
├── build.sh, flash_mcu.sh
└── arm-none-eabi-toolchain.cmake
```

---

## Notes

이 프로젝트는 **학습 목적**입니다.
- `tests/vectors/rsa_test_key.pem` 은 테스트 전용 키 — production 사용 금지
- 실제 production secure boot 는 hardware root of trust (eFuse SRK, HAB), anti-rollback counter,
  image encryption 등 추가 필요
- 직접 구현은 학습 가치가 크지만, 실제 제품에선 검증된 crypto library (mbedTLS, BearSSL) 권장

_License: Educational use only. Not for production._
