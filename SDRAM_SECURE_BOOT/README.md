# SDRAM Secure Boot — i.MX RT1020 베어메탈 구현

> 베어메탈 환경에서 SHA-256 과 RSA-2048 을 외부 라이브러리 없이 직접 구현한
> multi-stage secure boot 프로젝트입니다. Stage 1 → Stage 2 → App(A/B) 로 이어지는
> 신뢰 사슬(chain of trust)에 더해, 무중단 A/B 업데이트와 RSA 서명 기반의
> anti-rollback 정책까지 다룹니다. 타깃 보드는 Cortex-M7 기반의 NXP i.MX RT1020 입니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![crypto](https://img.shields.io/badge/crypto-SHA--256_%2B_RSA--2048-orange)]()
[![chain](https://img.shields.io/badge/chain-Stage1%E2%86%92Stage2%E2%86%92App-blueviolet)]()
[![tests](https://img.shields.io/badge/host_UT-37%2F37_PASS-green)]()
[![deps](https://img.shields.io/badge/crypto_deps-0-success)]()

---

## 한눈에

i.MX RT1020 (Cortex-M7) 을 대상으로 한 multi-stage secure boot 입니다. 각 단계가
다음 단계를 RSA 로 검증한 뒤에야 그쪽으로 점프하는 **재귀적 verifier 패턴**을
따릅니다. 여기에 A/B 파티션, 버전 기반 우선순위, RSA 로 서명한 메타데이터 기반의
anti-rollback 까지 더해서 실무에 가까운 보안 구조를 갖췄습니다.

주요 특징입니다.

- **외부 암호 라이브러리를 쓰지 않습니다.** SHA-256, bignum 산술, RSA-2048 을 모두 직접 구현했습니다.
- **3단 신뢰 사슬을 구성합니다.** `ROM → Stage 1 → Stage 2 → App` 의 각 단계가 다음 단계를 RSA 로 인증합니다.
- **A/B 파티션과 fallback 을 갖췄습니다.** 한쪽 파티션이 손상되어도 다른 쪽에서 부팅이 계속됩니다.
- **Anti-rollback 정책을 적용했습니다.** `min_acceptable_version` 으로 취약점이 있던 옛 버전을 거부합니다.
- **이중 메타데이터와 atomic switch 가 들어갑니다.** 업데이트 도중에 전원이 끊겨도 항상 한쪽은 정상 상태로 남습니다.
- **메타데이터에도 RSA 서명을 적용했습니다.** 정책 자체를 위조할 수 없습니다.
- **호스트 서명 파이프라인이 빌드 시스템에 통합되어 있습니다.** `./build.sh` 한 번이면 빌드와 서명이 모두 끝납니다.
- **공격과 실패 시나리오 7가지를 보드에서 직접 시연합니다** (1비트 변조, downgrade 시도, 메타데이터 변조, fail-safe halt 등).

---

## 시작하기

```bash
./build.sh           # Stage 1, Stage 2, App A/B 를 빌드하고 자동으로 서명합니다
./flash_mcu.sh all   # 네 개의 이미지를 모두 보드에 올립니다

# Anti-rollback 정책 설정 (이중 메타데이터 + RSA 서명)
python3 tools/set_metadata.py --seq 1 --min-version 1
./flash_mcu.sh metadata
```

호스트 단위 테스트입니다.

```bash
cd tests && ./build.sh && ./test.sh   # 37/37 통과
```

---

## 문서

| 문서 | 내용 |
|---|---|
| [Architecture](docs/ARCHITECTURE.md) | 전체 구조, 메모리 맵 (App A/B 와 이중 메타데이터 포함), 부팅 흐름, 이미지와 메타데이터 레이아웃, 소프트웨어 사슬의 하드웨어 의존 한계 |
| [Trust Chain](docs/TRUST_CHAIN.md) | `verify_image` 의 6단계, 메타데이터 계층 (RSA 서명), A/B 선택, anti-rollback, 메타데이터 rollback 의 한계 |
| [Crypto Implementation](docs/CRYPTO_IMPLEMENTATION.md) | SHA-256, 2048비트 bignum 산술, modular exponentiation, PKCS#1 v1.5 |
| [Build & Testing](docs/BUILD_AND_TESTING.md) | 서명 파이프라인, flash 대상, 7가지 시나리오 결과 (서명 변조, downgrade, atomic switch, 메타데이터 검증, fail-safe 등) |

---

## 현재 상태

| 항목 | 상태 |
|---|---|
| 3단 신뢰 사슬 보드 검증 | ✅ ROM 부터 App 까지 정상 도달 |
| 이미지 위조 차단 시연 | ✅ 서명 1비트 변조 시 halt |
| A/B fallback 시연 | ✅ B 손상 시 A 로 자동 전환 |
| Anti-rollback 시연 | ✅ min=3 으로 설정하면 v1 과 v2 가 모두 거부됨 |
| Atomic switch 시연 | ✅ Primary 손상 시 Backup 이 자동 채택 |
| 메타데이터 RSA 검증 시연 | ✅ 서명 1비트 변조 시 BAD_SIGNATURE 표시 |
| Fail-safe halt 시연 | ✅ 양쪽 메타데이터가 모두 무효일 때 정지 |
| 호스트 단위 테스트 | ✅ 37 / 37 통과 (sha 6 + bignum 26 + rsa 5) |
| 빌드 자동화 | ✅ CMake POST_BUILD 로 서명까지 통합 |

---

## 프로젝트 구조

```
SDRAM_SECURE_BOOT/
├── bootloader/
│   ├── stage1/                     # 24KB. 변경 불가 verifier 1
│   └── stage2/                     # 256KB. verifier 2 (A/B 선택 + anti-rollback + 메타데이터 처리)
├── app/
│   ├── app_a/                      # 256KB 응용 파티션 A (버전 1)
│   └── app_b/                      # 256KB 응용 파티션 B (버전 2)
├── shared/
│   ├── include/{bignum.h, sha256.h, rsa.h, verify.h, metadata.h, embedded_pubkey.h, ...}
│   └── src/{bignum.c, sha256.c, rsa.c, verify.c, metadata.c, string_min.c, ...}
├── tests/                          # 호스트 단위 테스트 (Unity) 와 Python reference vectors
├── tools/
│   ├── extract_pubkey.py           # PEM 에서 public modulus 를 뽑아 embedded_pubkey.h 로 변환
│   ├── patch_stage2_header.py      # 헤더 박기 + SHA + RSA 서명 첨부 (--version 인자)
│   └── set_metadata.py             # 메타데이터 sector binary 생성 (magic + seq + min_ver + RSA 서명)
├── docs/                           # 상세 문서 (위 표 참조)
├── build.sh, flash_mcu.sh
└── arm-none-eabi-toolchain.cmake
```

---

## 진행 상황

### 완료된 항목

- [x] Step 0~2: SHA-256, bignum, RSA-2048 직접 구현
- [x] Step 3~4: 호스트 서명 파이프라인 + Stage 1 → Stage 2 검증 E2E
- [x] C-1: App A/B 골격과 메모리 맵 정의
- [x] C-2: Stage 2 를 verifier 로 전환하고 verify 로직을 shared 모듈로 추출
- [x] C-3: A/B 파티션 + 버전 헤더 + fallback
- [x] C-4: Anti-rollback (`min_acceptable_version`, 평문 메타데이터)
- [x] C-5a: 이중 메타데이터 + atomic switch + magic + SHA-256 무결성
- [x] C-5b: 메타데이터에 RSA-2048 서명 적용 + 단계별 reason 출력 (BAD_MAGIC, BAD_SIGNATURE 가시화)

### 다음 단계 — 하드웨어 의존, 별도 마일스톤

- [ ] 메타데이터 anti-rollback (eFuse monotonic counter, TPM, RPMB 등)
- [ ] HAB (NXP 하드웨어 secure boot) 활성화 비교
- [ ] 성능 개선: `bn_mod` 를 Barrett reduction 으로 (약 7배 가속)

### 소프트웨어 사슬의 알려진 한계

이 프로젝트는 **소프트웨어 사슬** 안에서 할 수 있는 최선까지 구현했습니다. 이미지와
메타데이터 모두 RSA 로 무결성을 보장합니다. 다만 다음 두 계층은 **하드웨어 없이는
보장할 수 없습니다**.

1. **Stage 1 의 진정한 불변성** — eFuse SRK (HAB) 또는 ROM mask 가 있어야 합니다.
2. **메타데이터 anti-rollback** — eFuse monotonic counter, TPM, secure element, RPMB 같은 보안 저장소가 필요합니다.

학습용으로 보드 한 대만 다루는 환경에서는 eFuse 굽기가 비가역이라 보드를 영구히 못
쓰게 만들 위험이 있어, hardware root of trust 영역은 의도적으로 범위에서 제외했습니다.

결국 실무 수준의 secure boot 은 **소프트웨어 사슬과 hardware root of trust 의
결합** 으로 완성됩니다.

---

## 주의 사항

이 프로젝트는 학습 목적과 함께 실무 적용을 위한 사전 준비를 겸합니다.

- `tests/vectors/rsa_test_key.pem` 은 테스트 전용 키입니다. 실제 제품에는 절대 쓰지 마세요.
- 이미지 서명과 메타데이터 서명에 **같은 키를 재사용** 하고 있습니다 (학습 단순화를 위한 선택). 실무에서는 키 계층을 분리하는 편이 안전합니다.
- 직접 구현은 학습 가치가 크지만, 실제 제품에는 mbedTLS 나 BearSSL 같은 검증된 암호 라이브러리, 그리고 MCUboot 같은 검증된 secure bootloader 를 쓰는 편을 권합니다.
- Hardware root of trust 가 없으면 소프트웨어 사슬의 마지막 계층인 메타데이터 anti-rollback 을 완전히 막을 수 없습니다.

_License: Educational use only. Not for production._
