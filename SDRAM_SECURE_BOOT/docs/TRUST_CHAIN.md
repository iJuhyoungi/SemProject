# Trust Chain

Stage 1 의 `Stage2_Verify()` 는 다음 **6단계** 를 순서대로 검증합니다.
하나라도 실패하면 즉시 `return 0` → Stage 2 점프 안 함 → halt.

## 6단계 요약

| # | 단계 | 검증 대상 | 무엇을 막는가 | 분류 |
|---|---|---|---|---|
| 1 | Vector sanity | Stage 2 의 SP/PC 영역 유효성 | hard-fault 분기 | sanity |
| 2 | Magic | header `0x1C` == `0xDEADBEEF` | 형식 오류 / 비-secure image | 형식 |
| 3 | Size | `[0x100, 0x40000]` 범위 | OOB / 분기 폭증 | 형식 |
| 4 | CRC32 | code 영역 reflected CRC | 전송·저장 손상 | **무결성** |
| 5 | SHA-256 | code 의 cryptographic hash | 의도적 변조 | **무결성** |
| 6 | **RSA-2048** | signature 의 PKCS#1 v1.5 + hash 일치 | **위조 (다른 누군가의 image)** | **인증** |

핵심: **1~5 단계는 무결성** (누구나 재계산 가능), **6단계가 인증** (private key 소유자만 통과).

---

## 단계별 상세

### 1. Vector Sanity

```c
uint32_t sp = *(volatile uint32_t *)addr;        // Stage 2 의 initial SP
uint32_t pc = *(volatile uint32_t *)(addr + 4);  // Stage 2 의 Reset_Handler

if ((sp & 0xF0000000u) != 0x20000000u) return 0;  // SP 가 DTCM/OCRAM 안인가
if ((pc & 0xF0000000u) != 0x60000000u) return 0;  // PC 가 FLASH 안인가
if ((pc & 0x1u) != 0x1u)               return 0;  // PC 의 Thumb bit
```

가장 먼저 vector table 의 기본 sanity 확인. 완전히 비어있거나 (0xFFFFFFFF) 깨진 image
로의 점프를 1차 차단.

### 2. Magic Check

```c
uint32_t magic = *(volatile uint32_t *)(base + 0x1C);
if (magic != 0xDEADBEEF) return 0;
```

`0xDEADBEEF` 는 호스트 patcher 와 Stage 1 사이의 **약속**. 이 image 가 secure boot 형식을
따르는지 확인. 호스트 patcher 가 박지 않은 raw image 는 여기서 걸림.

### 3. Size Check

```c
uint32_t size = *(volatile uint32_t *)(base + 0x20);
if (size < 0x100 || size > 0x40000) return 0;
```

`size` 는 code 길이 (signature 제외). 비현실적으로 작거나 큰 값을 차단 — 이후 CRC/SHA
계산 범위의 안전성 보장.

### 4. CRC32 Check

```c
uint32_t expected_crc = *(volatile uint32_t *)(base + 0x24);
uint32_t computed_crc = CRC32_ComputeWithSkip((const uint8_t *)base, size, 0x24);
if (computed_crc != expected_crc) return 0;
```

- 알고리즘: 표준 IEEE 802.3 reflected CRC32 (poly `0xEDB88320`, init/xor-out `0xFFFFFFFF`)
- CRC 필드 자체 (`0x24` ~ `0x27`) 는 0 으로 간주하고 계산 (`Skip`)
- Python `zlib.crc32` 와 동일 → 호스트 patcher 와 일치

**의미**: "전송·저장 중 손상됐나?" 답변. 단, 의도적 변조는 CRC 재계산으로 우회 가능 →
다음 단계 (SHA, RSA) 가 필요한 이유.

### 5. SHA-256

```c
uint8_t img_hash[32];
SHA256_Compute((const uint8_t *)base, size, img_hash);
```

- code 영역 전체의 cryptographic hash 계산
- FIPS 180-4 표준 (자세한 구현은 [Crypto Implementation](CRYPTO_IMPLEMENTATION.md))

**의미**: "내용이 바뀌었나?" 답변. CRC 보다 강력 (충돌 저항). 단 SHA 도 누구나 계산 가능하므로
변조자가 SHA 값까지 갱신하면 무력 → RSA 가 필요한 이유.

### 6. RSA-2048 (인증)

```c
const uint8_t *signature = (const uint8_t *)(base + size);  // image 끝 256 byte
if (!rsa_verify_pkcs1_v15_sha256(img_hash, signature, EMBEDDED_PUBKEY_MODULUS)) {
    return 0;
}
```

검증 내부 흐름:
```
1. signature^65537 mod n  →  recovered EM (256 byte)
2. EM 의 PKCS#1 v1.5 padding 검사:
   0x00 || 0x01 || PS(0xFF×202) || 0x00 || DigestInfo(19B) || hash(32B)
3. EM 의 끝 32 byte (= recovered hash) == computed img_hash ?
```

**의미**: "진짜 개발자가 만든 image 인가?" 답변. signature 생성에 **private key** 가 필요한데,
보드엔 **public modulus 만** 있으므로 공격자가 signature 를 만들 수 없음.

→ 이 단계가 secure boot 를 "무결성 검증" 에서 "**진정한 인증**" 으로 격상.

---

## 무결성 vs 인증

| | 무결성 (CRC, SHA) | 인증 (RSA) |
|---|---|---|
| 질문 | "데이터가 손상/변조됐나?" | "누가 만들었나?" |
| 검증 키 | 없음 (누구나 재계산) | public key |
| 위조 방어 | ❌ (변조자가 재계산하면 우회) | ✅ (private key 없으면 불가) |
| 비용 | 낮음 (μs ~ ms) | 높음 (modexp 약 1.4 초) |

secure boot 는 **둘 다 필요**:
- CRC/SHA: 빠른 1차 필터 (손상·변조 검출)
- RSA: 최종 인증 (위조 차단)

---

## FAIL 경로 — Halt, No Recovery

```c
static void Halt_On_Verify_Fail(void)
{
    LED_On();                       // LED 상시 점등 = halt 상태
    __asm volatile("cpsid i");      // 인터럽트 비활성화
    while (1) { __asm volatile("wfi"); }
}
```

검증 실패 시 **recovery fallback 없이 halt**. 이유:
- 검증 실패 = 위조됐을 수 있는 image
- 그런 image 로는 **절대 점프하지 않는다** 가 secure boot 의 핵심 불변식
- LED 상시 점등으로 halt 상태를 정상 부팅 (Stage 2 깜빡임) 과 구분

production 환경에선 recovery image (golden image) fallback 을 둘 수 있지만,
이 학습 프로젝트는 "검증 실패 = 정지" 의 단순·엄격한 모델을 채택.

---

## 위조 차단 시연

signature 의 **단 1비트** 만 변조해도 검출됩니다 (CRC, SHA 는 통과):

```
[Verify] SHA-256: 85a150613f...        ← CRC, SHA 는 PASS (signature 영역 외부)
[Verify] RSA signature FAIL            ← 1비트 변조 검출
[BL1] Stage 2 INVALID - halting (no recovery)
```

RSA modexp 의 **avalanche effect** 때문 — signature 1비트 변경 → recovered EM 전체가
random 화 → PKCS#1 padding 의 첫 magic bytes (`0x00 0x01`) 부터 깨짐.

→ 결과·시연은 [Build & Testing](BUILD_AND_TESTING.md) 참조.
