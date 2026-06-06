# Trust Chain

Stage 1 과 Stage 2 의 verifier 는 같은 `verify_image()` (`shared/src/verify.c`) 를
사용해 **동일한 6단계** 를 수행합니다. Stage 2 는 여기에 더해 App 을 검증하기 전에
**메타데이터 기반의 정책 적용** (A/B 선택과 anti-rollback) 단계를 거칩니다.

## 전체 흐름

```
[Stage 1]                        [Stage 2]                          [App A/B]
  verify_image(Stage 2)            ① metadata_read_active
  = 6단계 trust chain                = 이중 메타데이터 검증, 큰 seq 채택
                                     - magic + RSA-2048 서명 검증
                                     - 둘 다 invalid 면 halt (fail-safe)
                                   ② min_acceptable_version 추출
                                   ③ A/B version 비교 → 우선순위 결정
                                   ④ primary anti-rollback 검사
                                      (version < min 이면 REJECTED)
                                   ⑤ verify_image(primary) — 6단계
                                   ⑥ 실패 시 secondary 로 fallback
                                      (④, ⑤ 반복)
```

---

## 6단계 — Stage 1 과 Stage 2 의 verify_image 공통

`shared/src/verify.c` 의 `verify_image(base, modulus)` 가 다음 6단계를 차례로
수행합니다. 한 단계라도 실패하면 즉시 `return 0` 으로 반환합니다.

| # | 단계 | 검증 대상 | 무엇을 막는가 | 분류 |
|---|---|---|---|---|
| 1 | Vector sanity | image 의 SP / PC 영역 유효성 | hard-fault 분기 | sanity |
| 2 | Magic | header `0x1C` == `0xDEADBEEF` | 형식 오류, 비-secure image | 형식 |
| 3 | Size | `[0x100, 0x40000]` 범위 | OOB, 분기 폭증 | 형식 |
| 4 | CRC32 | code 영역의 reflected CRC | 전송·저장 손상 | **무결성** |
| 5 | SHA-256 | code 의 cryptographic hash | 의도적 변조 | **무결성** |
| 6 | **RSA-2048** | signature 의 PKCS#1 v1.5 + hash 일치 | **위조 (다른 사람이 만든 image)** | **인증** |

핵심은 두 가지입니다. 1~5 단계는 **무결성** 검증 (누구나 재계산할 수 있습니다)이고,
6단계가 **인증** 입니다 (private key 가 있는 사람만 통과시킬 수 있습니다).

### 단계별 상세

#### 1. Vector sanity
```c
if ((sp & 0xF0000000u) != 0x20000000u) return 0;  // SP 가 DTCM / OCRAM 안인가
if ((pc & 0xF0000000u) != 0x60000000u) return 0;  // PC 가 FLASH 안인가
if ((pc & 0x1u) != 0x1u)               return 0;  // PC 의 Thumb 비트가 켜져 있는가
```

#### 2. Magic check
```c
if (*(uint32_t *)(base + 0x1C) != 0xDEADBEEF) return 0;
```

#### 3. Size check
```c
uint32_t size = *(uint32_t *)(base + 0x20);
if (size < 0x100 || size > 0x40000) return 0;
```

#### 4. CRC32 check
표준 IEEE 802.3 reflected CRC32 (다항식 `0xEDB88320`) 입니다. Python 의 `zlib.crc32`
와 같은 결과를 냅니다.

#### 5. SHA-256
FIPS 180-4 표준입니다. code 영역 전체에 대한 cryptographic hash 를 계산합니다.

#### 6. RSA-2048 (인증)
```
1. signature^65537 mod n  →  recovered EM (256 byte)
2. EM 의 PKCS#1 v1.5 padding 검사:
   0x00 || 0x01 || PS(0xFF×202) || 0x00 || DigestInfo(19B) || hash(32B)
3. EM 의 끝 32 byte (= 복구된 hash) == 계산한 img_hash 인가
```

private key 가 없으면 어떤 signature 도 만들어낼 수 없습니다. 그래서 이 단계가
"위조" 를 차단합니다.

---

## 메타데이터 계층 — Stage 2 의 정책 결정

Stage 2 는 App 을 검증하기 전에 **현재 정책을 메타데이터에서 결정** 합니다.

### 이중 메타데이터의 atomic switch

```c
// shared/src/metadata.c
int metadata_read_active(metadata_t *out,
                         metadata_reason_t *primary_reason,
                         metadata_reason_t *backup_reason);
```

흐름은 다음과 같습니다.

1. Primary 메타데이터 (`0x600C8000`) 를 검증합니다.
2. Backup 메타데이터 (`0x600C9000`) 를 검증합니다.
3. 둘 다 valid 면 `sequence_number` 가 큰 쪽을 채택합니다 (최신 정책).
4. 한쪽만 valid 면 그쪽을 채택합니다 (다른 쪽은 업데이트 도중 전원이 끊긴 상태).
5. 둘 다 invalid 면 `return 0` 으로 반환해 Stage 2 가 halt 합니다 (fail-safe).

각 메타데이터 검증 (magic + RSA-2048 signature) 의 결과는 `metadata_reason_t` enum
으로 호출자에 전달됩니다.

```c
typedef enum {
    METADATA_OK = 0,
    METADATA_BAD_MAGIC = 1,
    METADATA_BAD_SIGNATURE = 2,
} metadata_reason_t;
```

Stage 2 가 이 reason 을 단계별로 UART 에 출력하기 때문에 (`Primary: BAD_SIGNATURE / Backup: OK` 같은 식으로) 검증이 어디서 깨졌는지 보드 로그만 봐도 한눈에 들어옵니다. 시연과 디버깅에 모두 유용합니다.

### 메타데이터 무결성 — RSA-2048 서명

각 메타데이터 sector 의 구조입니다.

```
[0x00 .. 0x1F]  magic + seq + min_ver + reserved (32 byte, SHA-256 입력)
[0x20 .. 0x11F] RSA-2048 signature = PKCS#1 v1.5 ( SHA-256(header), privkey )
```

검증은 다음 순서로 이뤄집니다.

1. `magic == 0x5EC8B007` 인지 확인합니다.
2. SHA-256(header) 을 다시 계산합니다.
3. `rsa_verify_pkcs1_v15_sha256(SHA, signature, EMBEDDED_PUBKEY_MODULUS)` 로 서명을 검증합니다.

SHA 만 박아 두면 공격자가 메타데이터를 위조한 뒤 SHA 도 다시 계산해 박을 수 있어 통과해
버립니다. RSA 서명으로 봉인하면 private key 없이는 어떤 변조도 검증 단계에서 잡힙니다.

### Anti-rollback — App 에 적용되는 정책

채택된 메타데이터의 `min_acceptable_version` 으로 App 의 version 을 검사합니다.

```c
if (app_version < min_ver) {
    // REJECTED — downgrade 시도로 간주
} else {
    verify_image(app, modulus)  // 통과하면 점프
}
```

옛 펌웨어 (서명은 진짜) 가 들어와도 정책 미달이면 거부됩니다. 이것이 **downgrade
attack 차단** 의 본질입니다.

---

## A/B 파티션 선택 (Stage 2)

```c
uint32_t va = read_version(APP_A_BASE);  // header offset 0x28
uint32_t vb = read_version(APP_B_BASE);

if (vb > va) { primary = B; secondary = A; }
else         { primary = A; secondary = B; }

// primary 시도 (anti-rollback 검사 + verify)
if (prim_ver < min_ver) /* REJECTED */;
else if (verify_image(primary, ...)) jump_to_image(primary);

// 실패 시 secondary 로 fallback
if (sec_ver < min_ver) /* REJECTED */;
else if (verify_image(secondary, ...)) jump_to_image(secondary);

halt();
```

이렇게 하면 한쪽이 손상되어도 다른 쪽에서 부팅을 계속할 수 있습니다. 무중단 업데이트와
안전망을 동시에 제공합니다.

---

## 4개 계층의 보호 — 무결성 / 인증 / Anti-rollback / Atomic

| 계층 | 무엇을 막는가 | 검증 키 | 비용 |
|---|---|---|---|
| **무결성** (CRC, SHA) | 우연 손상, 단순 변조 | 없음 | μs ~ ms |
| **인증** (이미지의 RSA) | 위조 (다른 사람이 만든 이미지) | public key | 약 1.4 초 (modexp) |
| **Anti-rollback** (`min_ver`) | downgrade attack (옛 취약 펌웨어) | 메타데이터 안의 정책 | μs (정수 비교) |
| **Atomic** (이중 메타데이터의 RSA) | 메타데이터 변조, 업데이트 중 전원 끊김 | 메타데이터 자체의 RSA | 약 1.4 초 × 2 sector |

네 계층이 서로 다른 위협을 담당하면서 함께 작동합니다.

---

## 실패 경로

- **이미지 검증 실패**: halt (LED 상시 점등, 복구 없음).
- **메타데이터 둘 다 invalid**: halt (fail-safe — 정책을 못 읽으면 무조건 정지하는 편이 안전합니다).
- **A/B 둘 다 REJECTED 또는 검증 실패**: halt.

핵심 불변식은 다음 한 줄입니다. **위조됐을 가능성이 있는 이미지로는 절대 점프하지
않는다.**

---

## 소프트웨어 사슬의 한계 — 메타데이터 rollback

지금 구현으로는 막을 수 없는 시나리오가 있습니다.

### 메타데이터 rollback attack

```
공격자가 옛 메타데이터 (서명은 진짜) 를 보관하고 있다고 가정합니다.
  옛 metadata = {seq=10, min_ver=1, signed}
  현재 active = {seq=12, min_ver=3, signed}

공격 흐름은 다음과 같습니다.
  1. 보드의 두 메타데이터 sector 를 옛 metadata (seq=10) 로 덮어 씁니다.
  2. 재부팅 시 둘 다 RSA 검증을 통과합니다 (서명이 진짜라서). seq=10 이 채택됩니다.
  3. min_ver=1 이 적용되어 옛 v1 (취약) 펌웨어가 통과합니다.
```

근본 원인은 **소프트웨어 저장소는 언제든 erase 할 수 있다는 점** 입니다. 어떤 SW
구조로도 monotonic 을 보장할 수 없습니다.

### 실무에서의 해결 — Hardware Root of Trust

| 방식 | 어디 | 비고 |
|---|---|---|
| **eFuse monotonic counter** | NXP HAB (SRK revocation 영역) | 비가역. 보드를 잘못 굽히면 영구히 못 씁니다. |
| **TPM / Secure Element** | 별도의 보안 칩 (ATECC 같은 제품) | BOM 비용이 추가됩니다. |
| **TrustZone secure storage** | Cortex-M33 / M85 | 새로운 보드가 필요합니다. |
| **RPMB** | eMMC / UFS | 모바일 환경의 표준입니다. |

이 프로젝트는 소프트웨어 사슬에서 할 수 있는 최선까지 (이미지와 메타데이터 모두 RSA
무결성) 구현했습니다. 마지막 계층인 메타데이터 anti-rollback 은 하드웨어에 의존합니다.

실무 수준의 secure boot 은 결국 **소프트웨어 사슬과 hardware root of trust 의
결합** 으로 완성됩니다.

---

## 위조 차단 시연

### 이미지 signature 의 1비트 변조
```
[Verify] RSA signature FAIL            ← 1비트 변조도 잡힙니다
[BL1] Stage 2 INVALID - halting (no recovery)
```

### Downgrade 시도
```
[BL2] Min acceptable version = 0x00000003
[BL2] App B REJECTED - version below min (downgrade attempt)
[BL2] App A REJECTED - version below min
[BL2] No valid image - halting
```

### 메타데이터 RSA 검증 (Primary signature 1비트 XOR)
```
[BL2] Metadata Primary: BAD_SIGNATURE   ← RSA 단계에서 깨졌다는 표시
[BL2] Metadata Backup:  OK
[BL2] Metadata seq = 0x00000005          ← backup 이 자동 채택됩니다
```

### 메타데이터 sector 의 erase
```
[BL2] Metadata Primary: BAD_MAGIC        ← erase 결과는 0xFFFFFFFF 이라서 magic 불일치
[BL2] Metadata Backup:  OK
```

결과와 시연 절차는 [Build & Testing](BUILD_AND_TESTING.md) 에서 자세히 확인할 수 있습니다.
