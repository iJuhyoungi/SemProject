# Build & Testing

## 빌드와 서명 파이프라인

`./build.sh` 한 번이면 빌드와 서명이 모두 끝납니다.

```
build.sh
 ├─ 1) python3 tools/extract_pubkey.py       # PEM → shared/include/embedded_pubkey.h
 │       └ RSA modulus 를 C 헤더로 만듭니다 (Stage 1, 2 binary 에 임베드)
 │
 ├─ 2) cmake -B build + cmake --build build  # ARM cross-toolchain
 │       ├ Stage 1 (.elf) — embedded_pubkey.h 를 include 합니다
 │       ├ Stage 2 (.elf) → POST_BUILD:
 │       │   ├ objcopy .elf → .bin
 │       │   └ python3 tools/patch_stage2_header.py *.bin
 │       │       └ magic / size / CRC32 / version 박기 + SHA-256 + RSA 서명 첨부
 │       ├ App A (.elf) → POST_BUILD: 위와 동일, --version 1
 │       └ App B (.elf) → POST_BUILD: 위와 동일, --version 2
 │
 └─ 산출물: 서명된 Stage 1, Stage 2, App A, App B (.bin)
```

`set -e` 를 켜 두었기 때문에 어느 단계든 실패하면 즉시 중단됩니다. 검증되지 않은
이미지가 보드에 올라가는 사고를 막기 위함입니다.

메타데이터는 별도의 절차로 다룹니다 (정책 갱신이 필요할 때).

```bash
python3 tools/set_metadata.py --seq N --min-version M
./flash_mcu.sh metadata     # Primary 와 Backup 둘 다
```

## 호스트 도구

| 도구 | 역할 | 실행 빈도 |
|---|---|---|
| `tools/extract_pubkey.py` | PEM 에서 public modulus 를 추출해 `embedded_pubkey.h` 로 변환합니다 | 1회 (키를 바꿀 때만) |
| `tools/patch_stage2_header.py` | 헤더 (magic, size, CRC, version) + SHA + RSA 서명을 첨부합니다 | 매 빌드 (POST_BUILD 에서 자동 호출) |
| `tools/set_metadata.py` | 메타데이터 sector binary 를 생성합니다 (magic + seq + min_ver + RSA 서명) | 정책 갱신 시 |

## Flash

```bash
./flash_mcu.sh all       # Stage 1 + Stage 2 + App A + App B
./flash_mcu.sh stage1    # Stage 1 만
./flash_mcu.sh stage2    # Stage 2 만
./flash_mcu.sh app_a     # App A
./flash_mcu.sh app_b     # App B
./flash_mcu.sh apps      # App A + App B
./flash_mcu.sh metadata           # Primary + Backup 둘 다 (초기 setup 용)
./flash_mcu.sh metadata_primary   # Primary 만
./flash_mcu.sh metadata_backup    # Backup 만 (atomic 업데이트 용)
```

Flash 주소는 다음과 같습니다.

- Stage 1: `0x60002000` (vector 와 header 포함은 `0x60000000` 부터)
- Stage 2: `0x60008000`
- App A: `0x60048000`
- App B: `0x60088000`
- Metadata Primary: `0x600C8000`
- Metadata Backup: `0x600C9000`

### Atomic 메타데이터 업데이트 예시

```bash
# 현재 active 가 Primary (seq=10) 라고 가정합니다.
# Backup 만 갱신하면 다음 부팅 때 새 정책이 자동으로 활성화됩니다.
python3 tools/set_metadata.py --seq 11 --min-version 2
./flash_mcu.sh metadata_backup     # Backup 만. Primary 는 안전망으로 남습니다.
```

## 키 관리

| 항목 | 위치 | git | 비고 |
|---|---|---|---|
| Private key | `tests/vectors/rsa_test_key.pem` | ❌ `.gitignore` | 서명 권한입니다. 외부에 절대 유출되면 안 됩니다. |
| Public modulus | `shared/include/embedded_pubkey.h` | ✅ tracked | 자동으로 생성되고 Stage 1, 2 에 임베드됩니다. |

> 이 프로젝트의 PEM 은 **테스트 전용 키** 입니다. 실제 제품에는 절대 쓰지 마세요.
>
> 이미지 서명과 메타데이터 서명에 **같은 키를 재사용** 하고 있습니다 (학습 단순화를
> 위한 선택). 실무에서는 키 계층을 분리하는 편이 안전합니다 — 예를 들어 이미지 키가
> 유출되어도 메타데이터는 보호되도록 분리할 수 있습니다.

---

## 테스트

### 호스트 단위 테스트 (Unity)

```bash
cd tests
./build.sh    # 호스트 x86_64 gcc (RT toolchain 을 쓰지 않습니다)
./test.sh     # ctest -V
```

| Suite | Tests | 다루는 범위 |
|---|---|---|
| `sha256` | 6 | NIST FIPS 180-4 vectors (empty, "abc", 56-byte) + Streaming API 동치성 |
| `bignum` | 26 | add (3) + sub (2) + cmp (3) + bytes (2) + mul (6) + mod (5) + modexp (5) |
| `rsa` | 5 | 유효 서명 3개 + 변조된 서명 + 불일치 hash |
| **합계** | **37** | **전부 통과, 약 0.07 초** |

Reference 값은 다음과 같이 만들었습니다.

- Bignum / modexp: Python 의 `int` (임의 정밀도) 로 생성합니다.
- RSA: Python `cryptography` 로 keypair 와 signature 를 만듭니다.

호스트 단위 테스트의 가치는 분명합니다. 보드 cycle (build → flash → reset → UART
확인이 분 단위) 을 `ctest` (초 단위) 로 대체해 줍니다.

### 보드 smoke test (Stage 1)

```
[Selftest] SHA-256("abc")        : PASS
[Selftest] bn_add(1+2)           : PASS
[Selftest] bn_mul(1*1)           : PASS
[Selftest] bn_mod(5 mod 3)       : PASS
[Selftest] bn_modexp(2^10%1000)  : PASS
```

---

## 검증 결과

### 시나리오 1 — 정상 부팅 (3단 사슬과 정책 적용)

```
[BL1] Stage 1 immutable (Secure Boot)
[Selftest] ... (5개 PASS)
[BL1] Verifying Stage 2 ... RSA OK → jumping

[BL2] Stage 2 verifier running
[BL2] Metadata Primary: OK
[BL2] Metadata Backup:  OK
[BL2] Metadata seq = 0x00000005
[BL2] Min acceptable version = 0x00000001
[BL2] Trying App B ... RSA OK → jumping

[App B] running (slot B)        ← 사슬 끝까지 도달. LED 가 3회 깜빡입니다.
```

### 시나리오 2 — A/B fallback (App B 가 손상된 경우)

App B 의 signature 를 1비트 변조하면 B 검증이 실패하고 A 로 자동 전환됩니다.

```
[BL2] Trying App B ...
[Verify] RSA signature FAIL              ← B 손상이 잡힙니다
[BL2] Falling back to App A ...
[Verify] RSA signature OK
[BL2] App A OK - jumping

[App A] running (slot A)        ← A 로 fallback. LED 가 2회 깜빡입니다.
```

### 시나리오 3 — Downgrade 시도 차단

`min_acceptable_version = 3` 으로 설정해 두면 (A=v1, B=v2 인 상태에서) 다음과 같이
동작합니다.

```
[BL2] Metadata seq = 0x00000020
[BL2] Min acceptable version = 0x00000003
[BL2] Trying App B ...
[BL2] App B REJECTED - version below min (downgrade attempt)
[BL2] Falling back to App A ...
[BL2] App A REJECTED - version below min
[BL2] No valid image - halting
```

옛 펌웨어 (서명은 진짜) 라도 정책에 미달하면 거부됩니다. **Downgrade attack 차단을
직접 시연** 한 결과입니다.

### 시나리오 4 — Atomic switch (Primary erase)

```bash
pyocd erase --sector 0x600C8000 -t mimxrt1020   # Primary 만 erase
```

```
[BL2] Metadata Primary: BAD_MAGIC        ← erase 결과가 0xFFFFFFFF 이라 magic 불일치
[BL2] Metadata Backup:  OK
[BL2] Metadata seq = 0x00000005          ← Backup 이 자동으로 채택됩니다
[BL2] Trying App B ... → 부팅
```

한쪽이 손상되어도 다른 쪽으로 부팅이 이어집니다.

### 시나리오 5 — 메타데이터 RSA 검증 (signature 1비트 XOR)

```bash
python3 -c "buf = bytearray(open('build/metadata.bin','rb').read()); buf[0x20] ^= 0x01; open('build/metadata_tampered.bin','wb').write(bytes(buf))"
pyocd flash build/metadata_tampered.bin -t mimxrt1020 --base-address 0x600C8000
```

```
[BL2] Metadata Primary: BAD_SIGNATURE   ← RSA 단계에서 깨졌다는 표시가 명확히 보입니다
[BL2] Metadata Backup:  OK
[BL2] Metadata seq = 0x00000005          ← Backup 이 채택됩니다
```

이 출력은 **RSA verify 단계가 실제로 동작한다는 직접 증명** 입니다. SHA 만 가지고는
통과할 수 없는 상태입니다.

### 시나리오 6 — Fail-safe halt (둘 다 손상)

```bash
pyocd erase --sector 0x600C8000 -t mimxrt1020
pyocd flash build/metadata_tampered.bin -t mimxrt1020 --base-address 0x600C9000
```

```
[BL2] Metadata Primary: BAD_MAGIC
[BL2] Metadata Backup:  BAD_SIGNATURE
[BL2] Metadata both invalid - halting (fail-safe)
```

정책을 읽지 못하면 일단 정지합니다. fail-open 으로 가서 downgrade 우회를 허용하는
일이 없도록 막는 장치입니다.

### 시나리오 7 — Stage 2 의 signature 1비트 변조

```
[Verify] SHA-256: 85a150613f...   ← CRC, SHA 는 통과합니다 (signature 영역의 바깥이라서)
[Verify] RSA signature FAIL       ← 1비트 변조도 잡힙니다
[BL1] Stage 2 INVALID - halting (no recovery)
```

RSA modexp 의 **avalanche effect** 덕분입니다. signature 256 바이트 중 어느 한 비트라도
공격자가 추측해서 맞출 확률은 사실상 0 에 가깝습니다.

---

## CI 통합 가능성

`./build.sh` 와 `cd tests && ./test.sh` 만으로 빌드, 서명, 테스트가 모두 한 줄로
끝나기 때문에 GitHub Actions 같은 CI 에 그대로 통합할 수 있습니다.

- 호스트 단위 테스트 (`ctest`) 는 알고리즘 회귀를 자동으로 검증합니다.
- 보드 검증은 hardware-in-the-loop 가 필요해서 별도의 러너로 분리해야 합니다.

알고리즘 구현은 [Crypto Implementation](CRYPTO_IMPLEMENTATION.md) 을 참고해 주세요.
