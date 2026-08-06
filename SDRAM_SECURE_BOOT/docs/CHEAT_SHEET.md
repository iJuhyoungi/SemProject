# Cheat Sheet

이 프로젝트를 한 장으로 요약한 문서입니다. 오래 손을 뗐다가 돌아왔을 때, 또는 면접에서
설명할 때 먼저 펴 보는 용도입니다.

## 한 줄 정의

베어메탈 i.MX RT1020 에서 **SHA-256 과 RSA-2048 을 외부 라이브러리 없이 직접 구현한**
multi-stage secure boot 입니다. 여기에 A/B 업데이트, anti-rollback, measured boot,
키 계층과 폐기, fault injection 내성까지 얹었습니다.

## 메모리 맵

| 영역 | 주소 | 크기 | 서명 키 |
|---|---|---|---|
| Boot header (FCB/IVT) | `0x60000000` | 8 KB | — |
| **Stage 1** (verifier 1) | `0x60002000` | 24 KB | 서명 안 됨 (신뢰 기점, HAB 영역) |
| **Stage 2** (verifier 2) | `0x60008000` | 256 KB | **root** |
| **App A** (v1) | `0x60048000` | 256 KB | **release** |
| **App B** (v2) | `0x60088000` | 256 KB | **release** |
| Metadata Primary | `0x600C8000` | 4 KB | **root** |
| Metadata Backup | `0x600C9000` | 4 KB | **root** |
| Key Certificate | `0x600CA000` | 4 KB | **root** |

## 부팅 흐름

```
ROM → Stage 1 → Stage 2 → App A 또는 B

Stage 1: verify_image(Stage2, root) 2회 → 일치 확인 → jump (점프 직전 재판정)
Stage 2: 1. 메타데이터 검증 (root 서명, 이중 sector 중 큰 seq 채택)
         2. 키 인증서 검증 (root 서명) → release modulus 획득
         3. key_version >= min_key_version 확인 (폐기)
         4. A/B 우선순위 결정 (version 큰 쪽이 primary)
         5. version >= min_acceptable_version 확인 (downgrade)
         6. verify_image(App, release) 2회 → 판정·측정값 일치 확인
         7. 측정값 == 정책의 digest 확인 (measured boot)
         8. jump (점프 직전 재판정)
```

## 이미지 검증 6단계 (`verify_image`)

```
vector sanity → magic(0xDEADBEEF) → size → CRC32 → SHA-256 → RSA-2048
```

통과한 단계 수를 마지막에 대조합니다 (`volatile` 카운터). 결과는 토큰으로 반환합니다.

## 핵심 상수

| 이름 | 값 | 의미 |
|---|---|---|
| `IMG_MAGIC_VALUE` | `0xDEADBEEF` | 이미지 헤더 magic (vector reserved `0x1C`) |
| `METADATA_MAGIC` | `0x5EC8B007` | "SECBOOT" 비트변형 |
| `KEYCERT_MAGIC` | `0x4B435254` | "KCRT" |
| `SEC_PASS` | `0xA5C33C5A` | 판정 성공 토큰 |
| `SEC_FAIL` | `0x5A3CC3A5` | 판정 실패 토큰 (PASS 와 hamming distance 32) |
| `METADATA_HEADER_SIZE` | `0x60` | 정책 서명이 덮는 범위 |
| `KEYCERT_HEADER_SIZE` | `0x200` | 인증서 서명이 덮는 범위 |

## 이미지 헤더 (vector table reserved 재활용)

| 오프셋 | 내용 |
|---|---|
| `0x1C` | magic |
| `0x20` | size (code 길이) |
| `0x24` | CRC32 |
| `0x28` | version |
| `base+size` | RSA-2048 signature (256 byte) |

## 자주 쓰는 명령

```bash
# 빌드 (App 은 release 키로 자동 서명)
./build.sh
./build.sh -DSB_RELEASE_KEY_NAME=release_v2_private.pem    # 키 회전
./build.sh -DGLITCH_SIM_BITFLIP=ON                          # 글리치 모사 (값 손상)
./build.sh -DGLITCH_SIM_SKIP=ON                             # 글리치 모사 (분기 스킵)

# 플래시
./flash_mcu.sh all          # chip erase + stage1/2 + app A/B
./flash_mcu.sh stage2 | app_a | app_b | apps | metadata | keycert

# 정책과 인증서
python3 tools/set_metadata.py --seq 1 --min-version 1 --min-key-version 1
python3 tools/make_key_cert.py --key-id 1 --key-version 1

# 호스트 단위 테스트 (47건)
cd tests && ./build.sh && ./test.sh
```

**순서 주의**: App 을 다시 빌드하면 측정값이 바뀔 수 있으므로 메타데이터를 다시 만들어야
합니다. 키를 교체하면 인증서를 먼저 굽고 정책을 나중에 굽습니다.

## 키

```
~/.secure_boot_keys/          (SB_KEY_DIR 로 변경 가능, repo 밖)
├── root_private.pem          Stage 2 / 정책 / 인증서 서명. 거의 안 씀
└── release_vN_private.pem    App 서명. 매 릴리스. 인증서 재발급으로 교체 가능
```

`tests/vectors/rsa_test_key.pem` 은 호스트 UT 벡터 전용이며 보드 서명에 쓰지 않습니다.

## 위협 현황 (T-01 ~ T-20)

| 상태 | 개수 | 비고 |
|---|---|---|
| 차단 | 13 | 이 중 4건(T-10, T-12, T-16, T-20)은 보드에서 공격 재현 후 전후 대조 |
| 부분 | 1 | T-04 downgrade (T-05 로 우회 가능) |
| 열림 | 4 | T-05, T-08, T-09 는 **SW 로 불가** / T-19 는 잔여 위험 수용 |
| 범위 밖 | 2 | T-17 디버그 포트, T-18 부채널 |

## 면접에서 자주 나올 질문

**왜 RSA 인가?** 베어메탈에서 직접 구현하는 것이 학습 목표였습니다. ECDSA/EdDSA 는 타원곡선
계층이 깊어 라이브러리 의존이 불가피합니다. 트렌드는 ECDSA 이고 장기적으로는 PQC(ML-DSA)
입니다. 알고리즘 교체 문제이지 구조 문제가 아닙니다.

**왜 HAB 를 안 켰나?** eFuse SRK 굽기는 비가역입니다. 학습용 보드 한 대 환경에서 Closed
전환 후 실수는 영구 브릭입니다. 대신 어떻게 결합하는지를 설계 문서로 남겼고, Stage 2 이하
구조는 하드웨어 도입 후에도 그대로 재사용됩니다.

**FI 하드닝의 핵심은?** 크립토를 깨는 것보다 그 결론을 나르는 `if` 하나를 건드리는 쪽이
압도적으로 쉽습니다. 그래서 판정 값을 hamming distance 32 로 벌리고, 실패를 기본값으로
두고, 판정을 서로 떨어진 두 지점에서 합니다. 하드닝 전에는 서명이 깨진 이미지가 부팅되는
것을 보드 로그로 남겨두고 전후를 대조했습니다.

**하네스를 어떻게 설계했나?** "성공을 강제" 하게 만들면 어떤 방어도 뚫려서 효과를 측정할
수 없습니다. 실제 글리치가 할 수 있는 것(값 1비트 손상, 명령 1개 스킵)만 흉내내야 하드닝
후 같은 하네스가 실패하는 것이 방어의 증거가 됩니다.

**폐기는 어떻게 하나?** 옛 인증서를 지울 수 없으므로(flash 는 되돌릴 수 있음) 반대로
"지금 유효한 최소 세대"를 정책에 선언합니다. 정책은 root 서명 안에 있어 낮출 수 없습니다.

**남은 한계는?** 정책 자체의 rollback(T-05)과 Stage 1 불변성(T-08/09)은 SW 로 못 닫습니다.
되돌릴 수 없음을 소프트웨어가 만들어낼 수 없기 때문입니다. eFuse OTP 카운터와 HAB 가
필요합니다.

## 겪은 버그 (설명용)

| 버그 | 증상 | 교훈 |
|---|---|---|
| SHA-256 확장 루프 `t<63` | `state[0]`·`state[4]` 만 어긋남 | 출력 패턴으로 역추적 → W[63] 미초기화 |
| GCC 가 fill 루프를 `memset` 호출로 치환 | 베어메탈 link error | `-fno-tree-loop-distribute-patterns`, minimal libc 제공 |
| `sec_memeq` 마스크 `>> 31` | 함수가 **항상 PASS** (fail-open) | 플래그 비트 위치와 시프트 폭 불일치. UT 가 잡음 |
| `SEC_IS_PASS` 매크로 인자 이중 평가 | RSA 검증이 2회 실행 | 매크로 → `static inline` 함수 |
| 토큰 도입 후 `!rsa_verify(...)` 잔존 | **서명 검증 전체 무력화** | `SEC_FAIL` 은 0 이 아님. 반환 타입 변경 시 호출부 전수 grep |
| `METADATA_HEADER_SIZE` 만 옛 값 | 서명 검증 실패 → fail-safe halt | 레이아웃 상수와 구조체가 따로 놈. `_Static_assert` 로 잡을 수 있음 |

---

문서 전체는 [README](../README.md) 의 문서 표를 참고해 주세요.
