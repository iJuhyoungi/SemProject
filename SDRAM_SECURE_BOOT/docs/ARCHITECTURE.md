# Architecture

i.MX RT1020 (Cortex-M7) 을 대상으로 한 multi-stage secure boot 의 구조 문서입니다.
Stage 1 → Stage 2 → App(A/B) 로 이어지는 3단 신뢰 사슬과 함께, A/B 무중단 업데이트,
RSA 로 서명한 메타데이터 기반의 anti-rollback 까지 다룹니다.

## 왜 multi-stage 인가

Secure boot 의 신뢰는 **변경 불가능한 한 지점 (root of trust)** 에서 출발합니다. 그
위로 각 단계가 다음 단계를 검증해 나가는 **재귀적 verifier 패턴** 으로 사슬을 확장하는
구조입니다.

| 단계 | 역할 | 변경 가능성 |
|---|---|---|
| **Stage 1** | verifier 1 (검증자) | **변경 불가**. 이 단계가 위조되면 사슬의 뿌리가 무너집니다. |
| **Stage 2** | verifier 2 (재귀적 검증자 + A/B 선택 + anti-rollback) | 교체 가능합니다. 단 Stage 1 의 검증을 통과해야만 실행됩니다. |
| **App A / App B** | 최종 페이로드 (무중단 업데이트를 위한 A/B 파티션) | 교체 가능합니다. 단 Stage 2 의 검증을 통과해야만 실행됩니다. |

Stage 1 과 Stage 2 는 같은 `verify_image()` (`shared/src/verify.c`) 를 재사용합니다.
재귀적 검증 패턴이라 로직을 한곳에 모아 두고 두 단계가 함께 호출하는 식입니다 (DRY).
차이는 검증 대상뿐입니다 — Stage 1 은 Stage 2 를, Stage 2 는 App 을 검증합니다.

## 부팅 흐름

```
┌────────────────────────────────────────────────────────────┐
│ NXP Boot ROM (mask ROM, 변경 불가)                          │
│   - FlexSPI NOR flash 초기화                                 │
│   - XIP (eXecute In Place) 설정                              │
│   - 0x60002000 (Stage 1 vector table) 로 점프                │
└────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────┐
│ Stage 1 — 변경 불가 verifier                                 │
│   1. Reset_Handler → clock / UART / LED 준비                 │
│   2. Smoke selftest (SHA, bignum 핵심 함수 sanity)           │
│   3. verify_image(STAGE2_BASE, EMBEDDED_PUBKEY_MODULUS)     │
│      = 6단계 trust chain (별도 문서)                          │
│   4-a. 통과 → jump_to_image(0x60008000)                      │
│   4-b. 실패 → halt (LED 상시 점등, 복구 없음)                 │
└────────────────────────────────────────────────────────────┘
                          │ 검증 통과
                          ▼
┌────────────────────────────────────────────────────────────┐
│ Stage 2 — 재귀적 verifier (verifier 2)                       │
│   1. Reset_Handler → clock / UART / LED 준비                 │
│   2. metadata_read_active() — 이중 메타데이터 읽기           │
│      - magic + RSA-2048 서명 검증                            │
│      - 둘 다 valid 면 큰 sequence_number 채택                │
│      - 둘 다 invalid 면 halt (fail-safe)                     │
│   3. App A/B 의 version 을 읽고 min_acceptable_version 과 비교│
│   4. 우선순위 결정 (높은 version 이 primary, 나머지가 secondary)│
│   5. primary 시도:                                            │
│      - version < min 이면 REJECTED (downgrade 시도)          │
│      - 통과 시 verify_image(primary, ...) → 점프              │
│   6. 실패하면 secondary 로 fallback (같은 검사)               │
│   7. 둘 다 실패하면 halt                                     │
└────────────────────────────────────────────────────────────┘
                          │ 검증 통과
                          ▼
┌────────────────────────────────────────────────────────────┐
│ App A 또는 App B — 검증된 최종 페이로드                       │
│   - 자기 vector table 로 재초기화                            │
│   - 응용 코드 실행                                            │
└────────────────────────────────────────────────────────────┘
```

## 메모리 맵 (FlexSPI NOR Flash, XIP)

| 영역 | 주소 범위 | 크기 | 내용 |
|---|---|---|---|
| Boot header | `0x60000000` ~ `0x60001FFF` | 8 KB | FlexSPI config, IVT, boot data |
| **Stage 1** | `0x60002000` ~ `0x60007FFF` | 24 KB | 변경 불가 verifier 1 |
| **Stage 2** | `0x60008000` ~ `0x60047FFF` | 256 KB | 재귀적 verifier 2 (서명된 이미지) |
| **App A** | `0x60048000` ~ `0x60087FFF` | 256 KB | 응용 슬롯 A (서명, version 필드 포함) |
| **App B** | `0x60088000` ~ `0x600C7FFF` | 256 KB | 응용 슬롯 B (서명, version 필드 포함) |
| **Metadata Primary** | `0x600C8000` ~ `0x600C8FFF` | 4 KB | 정책 (magic + seq + min_ver + RSA-2048 서명) |
| **Metadata Backup** | `0x600C9000` ~ `0x600C9FFF` | 4 KB | Primary 의 안전망 (같은 형식) |

실행 시 RAM 사용입니다.

| 영역 | 용도 |
|---|---|
| DTCM / OCRAM (`0x2xxxxxxx`) | stack, .data, .bss |
| modexp 작업 그릇 | `bn_t` (256 B), `bn2_t` (512 B) on stack |

## 이미지 레이아웃 (Stage 2 / App A / App B 공통)

```
┌─────────────────────────────────────┬──────────────────────┐
│ code (size bytes)                   │ signature (256 bytes) │
│                                     │  RSA-2048 BE          │
│  offset 0x1C: magic = 0xDEADBEEF    │                       │
│  offset 0x20: size (code 길이)      │                       │
│  offset 0x24: CRC32 (code 영역)     │                       │
│  offset 0x28: version (uint32 LE)   │                       │
│  rest:        응용 코드               │                       │
└─────────────────────────────────────┴──────────────────────┘
↑ base                  ↑ base+size        ↑ base+size+256
```

- **magic, size, CRC32, version** 은 Cortex-M7 vector table 의 reserved slot (`0x1C ~ 0x2B`) 을 재활용합니다.
- **version** 은 호스트 patcher 가 `--version N` 으로 박습니다. App A 는 1, App B 는 2 처럼 다른 값을 주면 Stage 2 가 이 값을 비교해 우선순위를 정합니다.
- **signature** 는 이미지 끝에 256 바이트를 덧붙입니다. vector reserved 자리는 64 바이트뿐이라 서명이 들어가지 못합니다.

## 메타데이터 sector 레이아웃 (Primary / Backup 동일)

```
┌──────────────────────────────────────────────────────────────┐
│ [0x00] magic = 0x5EC8B007 ("SECBOOT" 비트변형)                │
│ [0x04] sequence_number (uint32 LE) — atomic switch 발행 번호  │
│ [0x08] min_acceptable_version (uint32 LE) — anti-rollback 정책│
│ [0x0C..0x1F] reserved (0xFF, SHA-256 입력에 포함)             │
│ [0x20..0x11F] RSA-2048 signature (256 byte)                  │
│   = PKCS#1 v1.5 ( SHA-256( [0x00..0x1F] ), private_key )     │
│ [0x120..0xFFF] reserved (0xFF)                               │
└──────────────────────────────────────────────────────────────┘
4 KB (flash sector 1개)
```

- **Atomic switch**: 업데이트 시 현재 active 가 아닌 쪽에 새 메타데이터를 씁니다. 쓰기가 끝나면 큰 `seq` 쪽이 자연스럽게 active 가 됩니다. 도중에 전원이 끊겨도 손대지 않은 쪽이 안전망 역할을 합니다.
- **RSA 서명**: SHA 만 박아 두면 공격자가 메타데이터를 위조한 뒤 SHA 도 다시 계산할 수 있습니다. PKCS#1 v1.5 RSA-2048 로 봉인하면 private key 없이는 어떤 변조도 통하지 않습니다.
- **검증 reason 가시화**: `metadata_reason_t` enum (`OK`, `BAD_MAGIC`, `BAD_SIGNATURE`) 로 Stage 2 가 단계별 진단을 UART 에 출력합니다. 어떤 단계에서 검증이 깨졌는지 한눈에 확인할 수 있습니다.

## 설계상의 결정들

1. **헤더 슬롯 재활용** — vector table 의 reserved 영역 (`0x1C`, `0x20`, `0x24`, `0x28`) 을 그대로 씁니다. 별도 헤더 영역을 새로 만들 필요가 없습니다.
2. **signature 는 이미지 뒤에 append** — vector reserved 자리가 64 바이트뿐이라 256 바이트 서명이 들어가지 못합니다. 이미지의 시작은 vector table 이어야 하므로 (진입점 규칙) 끝에 붙이는 쪽이 가장 깔끔합니다.
3. **size / CRC32 / SHA-256 의 대상은 code 영역만** — 서명 자체가 SHA(code) 의 결과라서, SHA 영역에 서명까지 포함하면 순환 의존이 생깁니다.
4. **재귀적 verifier 패턴** — Stage 1 과 Stage 2 가 같은 `verify_image()` 를 호출합니다. modulus 를 인자로 받게 만들어 두면 나중에 key hierarchy 를 도입하기도 쉽습니다.
5. **A/B 파티션** — 무중단 업데이트와 한쪽 손상 시의 fallback 을 함께 제공합니다. 우선순위는 version 으로 결정합니다.
6. **Anti-rollback** — 메타데이터 sector 의 `min_acceptable_version` 으로 downgrade 시도를 차단합니다.
7. **이중 메타데이터 + RSA 서명** — atomic switch (전원 끊김에 의한 영구 불능 방지) 와 완전한 무결성 (변조 차단) 을 함께 보장합니다.
8. **이미지 서명 키 재사용** — 메타데이터 서명에도 같은 키를 씁니다. 학습 단순화를 위한 선택이며, 실무에서는 key hierarchy 를 분리하는 편이 안전합니다.

## 소프트웨어 사슬의 한계 — Hardware Root of Trust 가 필요한 이유

이 프로젝트는 신뢰 사슬의 **소프트웨어 부분** 까지만 다룹니다. 다음 두 계층은 본질적으로
하드웨어에 의존합니다.

### 1. Stage 1 의 진정한 불변성

이 프로젝트에서 Stage 1 이 "변경 불가" 인 것은 사실상 **관례** 입니다 (재 flash 를 하지
않는다는 약속에 가깝습니다). 진짜로 변경 불가능하게 만들려면 다음과 같은 장치가
필요합니다.

- **eFuse SRK** (HAB 의 Super Root Key hash) — 한 번 굽고 나면 되돌릴 수 없습니다.
- **ROM mask** — 칩 제조 시점에 고정됩니다.

NXP HAB (Hardware-Assured Boot) 영역인데, 학습용으로 보드 한 대만 다루는 환경에서는
eFuse 굽기가 비가역이라 보드를 영구히 못 쓰게 만들 위험이 있습니다. 그래서 이 영역은
**의도적으로 범위에서 제외했습니다**.

### 2. 메타데이터 anti-rollback

현재 구현으로는 **메타데이터 자체의 anti-rollback 을 막을 방법이 없습니다**.

- 공격자가 옛 메타데이터 (서명은 진짜) 를 어딘가에 보관해 둡니다.
- 보드의 두 메타데이터 sector 를 옛 메타데이터로 덮어 씁니다.
- 부팅 시 양쪽 다 RSA 검증을 통과하니까 (서명이 진짜라서) 옛 `seq` 가 그대로 채택됩니다.
- 결과적으로 옛 `min_ver` 가 적용되어 옛 (취약) App 이 통과합니다.

근본 원인은 **소프트웨어 저장소는 언제든 erase 할 수 있다는 점** 입니다. 어떤 SW 구조도
monotonic 을 100% 보장할 수 없고, 보장 장치를 또 다른 SW 에 두면 그 SW 도 같은 공격에
노출됩니다 — 일종의 무한 회귀입니다.

실무에서는 다음과 같은 하드웨어 장치로 해결합니다.

- **eFuse OTP counter** — 한 번 굽힌 비트는 되돌릴 수 없으므로 monotonic 보장이 자연스럽습니다.
- **TPM / Secure Element** — 별도의 보안 칩에 monotonic counter 를 둡니다.
- **TrustZone secure storage** — Cortex-M33 / M85 의 secure world 를 활용합니다.
- **RPMB (Replay-Protected Memory Block)** — eMMC / UFS 에 정의된 모바일 표준입니다.

이 프로젝트는 소프트웨어 사슬에서 할 수 있는 최선 (이미지와 메타데이터 모두 RSA 무결성)
까지 구현했습니다. 마지막 계층인 메타데이터 anti-rollback 은 하드웨어가 함께 들어가야
완성됩니다.

자세한 신뢰 사슬 동작은 [Trust Chain](TRUST_CHAIN.md) 을 참고해 주세요.
