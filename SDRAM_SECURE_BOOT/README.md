# SDRAM Secure Boot — i.MX RT1020 베어메탈 구현

> 베어메탈 환경에서 SHA-256 과 RSA-2048 을 외부 라이브러리 없이 직접 구현한
> multi-stage secure boot 프로젝트입니다. Stage 1 → Stage 2 → App(A/B) 로 이어지는
> 신뢰 사슬(chain of trust)에 더해, 무중단 A/B 업데이트와 RSA 서명 기반의
> anti-rollback 정책까지 다룹니다. 타깃 보드는 Cortex-M7 기반의 NXP i.MX RT1020 입니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![crypto](https://img.shields.io/badge/crypto-SHA--256_%2B_RSA--2048-orange)]()
[![chain](https://img.shields.io/badge/chain-Stage1%E2%86%92Stage2%E2%86%92App-blueviolet)]()
[![tests](https://img.shields.io/badge/host_UT-47%2F47_PASS-green)]()
[![hardening](https://img.shields.io/badge/hardening-FI%20%2B%20measured%20boot%20%2B%20key%20revocation-red)]()
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
- **Fault injection 에 견디도록 하드닝했습니다.** 글리치를 모사해 하드닝 전에는 서명이 깨진 이미지가 부팅되는 것을 먼저 보이고, 방어 후 같은 공격이 막히는 것까지 보드에서 확인했습니다.
- **Measured boot 로 정책과 이미지를 묶었습니다.** 서명 두 개가 모두 진짜여도 세대가 어긋난 조합은 거부됩니다.
- **키를 교체할 수 있습니다.** root 가 발급한 인증서로 release 키를 인증하고, 정책의 `min_key_version` 으로 유출된 옛 키를 폐기합니다. Stage 1 은 건드리지 않습니다.
- **공격과 실패 시나리오를 보드에서 직접 시연합니다** (1비트 변조, downgrade, 메타데이터 변조, 글리치 2종, mix-and-match, 키 폐기, fail-safe halt).

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
cd tests && ./build.sh && ./test.sh   # 47/47 통과
```

---

## 문서

| 문서 | 내용 |
|---|---|
| [Cheat Sheet](docs/CHEAT_SHEET.md) | **한 장 요약** — 메모리 맵, 부팅 흐름, 상수, 자주 쓰는 명령, 면접 질문, 겪은 버그 |
| [Architecture](docs/ARCHITECTURE.md) | 전체 구조, 메모리 맵, 부팅 흐름, 이미지·메타데이터·인증서 레이아웃, 소프트웨어 사슬의 하드웨어 의존 한계 |
| [Trust Chain](docs/TRUST_CHAIN.md) | `verify_image` 의 6단계, 메타데이터 계층, A/B 선택, anti-rollback, 보호 계층별 담당 위협과 실패 경로 |
| [Threat Model](docs/THREAT_MODEL.md) | 보호 자산, 신뢰 경계, 공격자 모델, **위협 T-01 ~ T-20 과 각각의 상태** |
| [Fault Injection](docs/FAULT_INJECTION.md) | 글리치 모사 하네스 설계, **하드닝 before/after 보드 로그**, 적용한 방어 9가지 |
| [Key Management](docs/KEY_MANAGEMENT.md) | root/release 키 계층, 인증서 포맷, 키 회전과 폐기 절차 |
| [HAB and OTP](docs/HAB_AND_OTP.md) | 하드웨어 root of trust 설계 — HAB/SRK, OTP monotonic counter (퓨즈는 굽지 않음) |
| [Crypto Implementation](docs/CRYPTO_IMPLEMENTATION.md) | SHA-256, 2048비트 bignum 산술, modular exponentiation, PKCS#1 v1.5 |
| [Build & Testing](docs/BUILD_AND_TESTING.md) | 서명 파이프라인, flash 대상, 시나리오별 결과 |

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
| **Fault injection 하드닝** | ✅ 값 손상·분기 스킵 두 경로 모두 before/after 보드 로그 확보 |
| **Measured boot 시연** | ✅ 서명 두 개가 모두 진짜여도 조합이 어긋나면 거부 |
| **키 회전·폐기 시연** | ✅ v2 로 회전 후 진짜 v1 인증서 롤백이 `Key REVOKED` 로 차단 |
| 호스트 단위 테스트 | ✅ 47 / 47 통과 (sha 6 + bignum 26 + rsa 5 + secure 10) |
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
│   ├── include/{bignum.h, sha256.h, rsa.h, verify.h, metadata.h, keycert.h, secure.h, glitch.h, ...}
│   └── src/{bignum.c, sha256.c, rsa.c, verify.c, metadata.c, keycert.c, string_min.c, ...}
├── tests/                          # 호스트 단위 테스트 (Unity) 와 Python reference vectors
├── tools/
│   ├── extract_pubkey.py           # PEM 에서 public modulus 를 뽑아 embedded_pubkey.h 로 변환
│   ├── patch_stage2_header.py      # 헤더 박기 + SHA + RSA 서명 첨부 (--version 인자)
│   ├── set_metadata.py             # 정책 sector 생성 (seq + min_ver + min_key_ver + App 측정값 + root 서명)
│   └── make_key_cert.py            # 키 인증서 생성 (release 공개키를 root 로 인증)
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

### 하드닝 (보안 방점)

- [x] H-0: 위협모델 문서화 — 자산, 신뢰 경계, 공격자 모델, 위협 T-01 ~ T-20
- [x] H-1: Fault injection 하드닝 — 글리치 모사 하네스, 판정 토큰, 기본값 FAIL, 단계·루프 카운터, 상수시간 비교, 판정 이중화
- [x] H-2: Measured boot — 정책 서명이 App 의 SHA-256 측정값까지 덮음 (mix-and-match 차단)
- [x] H-3: 키 계층 + 폐기 — root/release 분리, root 서명 인증서, `min_key_version` 으로 폐기
- [x] H-4: HAB/SRK + OTP monotonic counter 설계 문서 (퓨즈는 굽지 않음)
- [x] H-5: 문서 봉인 — 위협 상태 갱신, 메모리 맵·신뢰 사슬 반영, cheat sheet

### 다음 단계 — 하드웨어 의존, 별도 마일스톤

- [ ] 메타데이터 anti-rollback 실제 적용 (eFuse monotonic counter) — 설계는 [HAB and OTP](docs/HAB_AND_OTP.md) 에 완료
- [ ] HAB 활성화 (SRK 퓨즈 + SEC_CONFIG Closed) — 개발용 보드 별도 확보 후
- [ ] 성능 개선: `bn_mod` 를 Barrett reduction 으로 (약 7배 가속)

### 소프트웨어 사슬의 알려진 한계

이 프로젝트는 **소프트웨어 사슬** 안에서 할 수 있는 최선까지 구현했습니다. 위협 20건 중
13건을 차단했고, 그중 4건은 보드에서 공격을 재현해 전후를 대조했습니다. 다만 다음 두
계층은 **하드웨어 없이는 보장할 수 없습니다**.

1. **Stage 1 의 진정한 불변성** — eFuse SRK (HAB) 또는 ROM mask 가 있어야 합니다.
2. **정책 자체의 anti-rollback** — 옛 정책과 옛 인증서를 함께 되돌리면 폐기가 무력화됩니다. eFuse monotonic counter, TPM, RPMB 같은 되돌릴 수 없는 저장소가 필요합니다.

공통 원인은 하나입니다. **되돌릴 수 없음(irreversibility)을 소프트웨어가 만들어낼 수
없습니다.** 보장 장치를 또 다른 소프트웨어에 두면 그 소프트웨어도 같은 공격에 노출되는
무한 회귀에 빠집니다.

학습용으로 보드 한 대만 다루는 환경에서는 eFuse 굽기가 비가역이라 보드를 영구히 못
쓰게 만들 위험이 있어, hardware root of trust 영역은 의도적으로 범위에서 제외했습니다.
**어떻게 결합하는지는 [HAB and OTP](docs/HAB_AND_OTP.md) 에 설계 수준으로 정리했고,
Stage 2 이하의 구조는 하드웨어를 도입해도 그대로 재사용됩니다.**

결국 실무 수준의 secure boot 은 **소프트웨어 사슬과 hardware root of trust 의
결합** 으로 완성됩니다.

---

## 주의 사항

이 프로젝트는 학습 목적과 함께 실무 적용을 위한 사전 준비를 겸합니다.

- **private key 는 repo 안에 두지 않습니다.** `SB_KEY_DIR` (기본 `~/.secure_boot_keys`) 에 보관하며, 실무에서는 root 키를 HSM 이나 오프라인 서명 서버에 둡니다. `tests/vectors/rsa_test_key.pem` 은 호스트 단위 테스트 벡터 전용이고 보드 서명에 쓰지 않습니다.
- **root 키가 유출되면 이 구조로는 복구할 수 없습니다.** 가장 약한 고리이며 의도적 범위 제한입니다. 실무에서는 HAB 의 SRK 슬롯 4개와 `SRK_REVOKE` 퓨즈로 완화합니다.
- 직접 구현은 학습 가치가 크지만, 실제 제품에는 mbedTLS 나 BearSSL 같은 검증된 암호 라이브러리, 그리고 MCUboot 같은 검증된 secure bootloader 를 쓰는 편을 권합니다.
- Fault injection 방어는 **단일 글리치 내성**까지를 목표로 합니다. 다중 글리치는 범위 밖입니다.
- 검증을 2회 독립 실행하므로 **부팅 시간이 약 두 배**입니다. 보안을 위해 받아들인 트레이드오프입니다.

_License: Educational use only. Not for production._
