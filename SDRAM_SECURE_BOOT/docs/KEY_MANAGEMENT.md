# Key Management

서명 키를 root 와 release 두 계층으로 나누고, 인증서와 폐기 정책으로 **키를 교체할 수 있게**
만든 구조를 정리한 문서입니다. [Threat Model](THREAT_MODEL.md) 의 위협 T-16 에 대응합니다.

## 왜 계층이 필요했는가

하드닝 전에는 키가 하나였습니다. 그 하나로 Stage 2 이미지, App 이미지, 정책 메타데이터를
모두 서명했습니다. 문제는 "키가 하나"라는 것 자체가 아니라 **그 하나가 절대 바뀔 수 없다**는
데 있었습니다.

공개키가 Stage 1 코드 안에 박혀 있으니, 서명 키를 바꾸려면 Stage 1 을 다시 구워야 합니다.
그런데 실제 제품에서 Stage 1 은 HAB/eFuse 로 고정되는 **바꿀 수 없는 것**입니다. 결론은
이렇게 됩니다.

> 서명 키가 유출되면 그 기기는 영원히 공격자 서명을 신뢰합니다.

한편 서명 키는 자주 쓰입니다. 빌드 서버가 매 릴리스마다 접근하고, CI 에 물려 있고, 개발자
PC 를 거칩니다. **가장 바꿀 수 없는 키를 가장 자주 쓰고 있던 것**이 문제의 핵심이었습니다.

## 키 역할

| 키 | 서명 대상 | 노출 빈도 | 보관 | 교체 가능성 |
|---|---|---|---|---|
| **Root** | Stage 2 이미지, 정책 메타데이터, **키 인증서** | 매우 낮음 (키 발급 때만) | 오프라인 / HSM 가정 | 사실상 불가 (신뢰 기점) |
| **Release** | App A / App B 이미지 | 높음 (매 릴리스) | 빌드 시스템 | **인증서 재발급으로 교체** |

**Stage 2 를 release 키로 서명하지 않는 이유**가 있습니다. Stage 2 는 검증자입니다. 검증자를
릴리스 키로 서명하면 릴리스 키 유출이 곧 검증자 교체 권한이 되어, 공격자가 검증 자체를
무력화한 Stage 2 를 심을 수 있습니다. 그러면 계층을 나눈 의미가 사라집니다. **자주 바뀌는
것(App)만 아래 계층으로 내립니다.**

## 키 보관

private key 는 **프로젝트 디렉토리 밖**에 둡니다.

```
~/.secure_boot_keys/          (chmod 700)
├── root_private.pem          (chmod 600)
├── release_v1_private.pem
└── release_v2_private.pem
```

환경변수 `SB_KEY_DIR` 로 위치를 바꿀 수 있고, 모든 호스트 도구와 CMake 가 이 값을 따릅니다.

`.gitignore` 의 `*.pem` 으로 거르는 방법도 있지만 그것만으로는 부족합니다. **패턴을 고치거나
`git add -f` 한 번이면 무너지는 방어**입니다. 애초에 repo 안에 없으면 실수할 여지 자체가
사라집니다.

`tests/vectors/rsa_test_key.pem` 은 남아 있지만 **호스트 단위 테스트 벡터 생성 전용**입니다.
보드에 올라가는 어떤 것도 이 키로 서명하지 않습니다.

> 실무에서는 root private key 를 HSM 이나 오프라인 서명 서버에 두고, 키 파일 자체가 사람
> 손에 닿지 않게 합니다. 이 프로젝트는 그 배치를 파일 시스템 수준으로 흉내낸 것입니다.

## 키 인증서

release 공개키를 **코드가 아니라 데이터로** flash 에 두고, root 서명으로 진정성을 보장합니다.

```
Key Certificate sector — 0x600CA000 (4 KB)

[0x000] magic = 0x4B435254 ("KCRT")
[0x004] key_id        (uint32 LE)
[0x008] key_version   (uint32 LE)   <- 폐기 기준
[0x00C] reserved0
[0x010] release public modulus (256 byte, big-endian)
[0x110] reserved1 (240 byte)
------- 여기까지 0x200 = root 서명 대상 -------
[0x200] root signature (256 byte, RSA-2048 PKCS#1 v1.5)
[0x300] 0xFF padding
```

modulus 를 big-endian 바이트열로 둔 이유는 보드에 이미 있는 `bn_from_bytes_be()` 를 그대로
쓰기 위해서입니다. RSA 서명도 같은 형식이라 일관됩니다.

## 신뢰 사슬

```
┌──────────────────────────────────────────────────────────┐
│ Root public modulus — Stage 1·2 코드에 임베드              │
│   (EMBEDDED_ROOT_MODULUS, tools/extract_pubkey.py 가 생성) │
└───────────────┬──────────────────────────────────────────┘
                │ root 서명 검증
       ┌────────┼─────────────────┬──────────────────┐
       ▼        ▼                 ▼                  ▼
  Stage 2   정책 메타데이터    키 인증서
  이미지    (min_ver,          (release 공개키,
            min_key_version)    key_id, key_version)
                                      │
                                      │ 인증서에서 꺼낸 release modulus
                                      ▼
                              App A / App B 이미지
```

Stage 2 의 검증 순서입니다.

1. 정책 메타데이터를 root 키로 검증 (`metadata_read_active`)
2. 키 인증서를 root 키로 검증 (`keycert_load`)
3. `key_version >= min_key_version` 확인 — **폐기 검사**
4. 인증서에서 꺼낸 release modulus 로 App 이미지 검증
5. App 측정값이 정책의 값과 일치하는지 확인 (measured boot)

## 키 회전 절차

release 키가 유출되었거나 정기 교체 시점일 때입니다.

```bash
# 1. 새 release 키 생성
openssl genrsa -out ~/.secure_boot_keys/release_v2_private.pem 2048
chmod 600 ~/.secure_boot_keys/release_v2_private.pem

# 2. App 을 새 키로 서명해 빌드
./build.sh -DSB_RELEASE_KEY_NAME=release_v2_private.pem
./flash_mcu.sh apps

# 3. root 가 새 인증서를 발급
python3 tools/make_key_cert.py --key-id 2 --key-version 2 \
        --release-key ~/.secure_boot_keys/release_v2_private.pem
./flash_mcu.sh keycert

# 4. 정책에서 옛 세대를 폐기
python3 tools/set_metadata.py --seq 2 --min-version 1 --min-key-version 2
./flash_mcu.sh metadata
```

**Stage 1 은 한 번도 건드리지 않습니다.** 이것이 계층을 나눈 이유 전부입니다.

순서가 중요합니다. 인증서를 먼저 굽고 정책을 나중에 굽습니다. 반대로 하면 한 번 halt 하지만,
인증서를 굽는 것으로 바로 복구됩니다.

## 폐기 — 지우는 게 아니라 선언하는 것

키를 새로 발급해도 **옛 인증서는 여전히 진짜**입니다. root 가 서명했고 그 서명은 지금도
유효합니다. 유출된 옛 키를 가진 공격자가 보관해 둔 옛 인증서를 되돌리면, 서명 검증만으로는
아무것도 걸러지지 않습니다.

flash 는 언제든 다시 쓸 수 있으므로 "옛 인증서를 지운다" 는 보장이 성립하지 않습니다.
그래서 반대로 접근합니다.

> **지금 유효한 최소 세대를 정책에 선언한다.**

정책은 root 서명 안에 있어 공격자가 이 숫자를 낮출 수 없습니다. 옛 인증서를 아무리 다시
구워도 `key_version` 이 기준보다 낮으면 거부됩니다.

이미지에 `min_acceptable_version` 이 필요했던 것과 정확히 같은 구조입니다. **키에도
anti-rollback 이 필요합니다.**

## 보드 검증 결과

### 인증서 위조 시도 — root 키 없는 공격자

공격자가 자기 키쌍을 만들어 인증서에 심고 자기 키로 서명한 경우입니다.

```
[BL2] Key cert: BAD_SIGNATURE
[BL2] Key certificate invalid - halting (fail-safe)
```

root 서명이 modulus 필드까지 덮으므로, root private key 없이는 어떤 조합도 만들 수
없습니다.

### 키 회전 — 옛 키로 서명된 이미지가 무효화됨

root 키로 다른 release 키에 인증서를 발급한 직후입니다 (앱은 아직 옛 키 서명).

```
[BL2] Key cert: OK                                   ← 새 키가 정식 인증됨
[BL2] App B REJECTED - signature verdict not PASS    ← 옛 키 서명은 무효
[BL2] App A REJECTED - signature verdict not PASS
[BL2] No valid image - halting
```

### 폐기 — 진짜 인증서를 되돌려도 거부

v2 로 회전한 뒤 **root 가 발급한 진짜 v1 인증서**를 되돌린 경우입니다.

```
[BL2] Key cert: OK                      ← 서명은 완벽히 유효합니다
[BL2] Key id = 0x00000001  version = 0x00000001
[BL2] Min Key Version = 0x00000002
[BL2] Key REVOKED - key_version below policy minimum
```

**서명 검증만으로는 원리적으로 막을 수 없는 공격입니다.** 공격자가 위조한 게 아니라 제조사가
과거에 정당하게 발급한 인증서를 되돌린 것이기 때문입니다. 정책에 선언한 최소 세대가 이걸
막았습니다.

## 남는 한계

### 1. 정책 자체의 rollback (T-05)

옛 정책(`min_key_version = 1`, 서명 진짜)과 옛 인증서를 **함께** 되돌리면 원상복구됩니다.
메타데이터의 `seq` 가 flash 에 있어 되돌릴 수 있기 때문입니다.

되돌릴 수 없는 하드웨어 카운터(eFuse OTP, TPM, RPMB)가 사슬에 들어와야 끊깁니다.
[HAB and OTP Design](HAB_AND_OTP.md) 에서 다룹니다.

### 2. Root 키 유출

root 키가 유출되면 이 구조로는 복구할 수 없습니다. 공격자가 자기 release 키에 유효한
인증서를 발급하고 정책까지 새로 서명할 수 있기 때문입니다.

실무에서는 이걸 두 가지로 완화합니다.

- **root 키를 여러 개 두고 폐기 가능하게** — i.MX HAB 는 SRK 슬롯 4개를 지원하고, 퓨즈로
  개별 폐기가 가능합니다. 하나가 유출되면 다음 슬롯으로 넘어갑니다.
- **root 키를 HSM 밖으로 내보내지 않음** — 서명은 HSM 안에서만 수행하고 키 자체는 추출
  불가능하게 만듭니다.

이 프로젝트는 SW 범위라 root 키가 단일이고 파일로 존재합니다. **가장 약한 고리이며, 의도적
범위 제한입니다.**

### 3. 인증서 계층의 깊이

여기서는 root → release 2단계입니다. 실무에서는 root → intermediate → release 처럼 더
깊게 두어, intermediate 를 제품군이나 지역 단위로 분리하기도 합니다. 원리는 같고 검증이
재귀적으로 반복될 뿐입니다.

---

관련 문서입니다. 공격자 모델은 [Threat Model](THREAT_MODEL.md), 검증 단계의 상세는
[Trust Chain](TRUST_CHAIN.md), fault injection 하드닝은
[Fault Injection](FAULT_INJECTION.md) 을 참고해 주세요.
