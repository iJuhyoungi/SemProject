# Architecture

i.MX RT1020 (Cortex-M7) 의 2-stage secure boot 구조.

## Why 2-Stage?

Secure boot 의 신뢰는 **변경 불가능한 한 지점 (root of trust)** 에서 출발해야 합니다.

| Stage | 역할 | 변경 가능성 |
|---|---|---|
| **Stage 1** | 검증자 (verifier) | **immutable** — 검증 로직이 위조되면 trust chain 의 root 가 무너짐 |
| **Stage 2** | 검증 대상 (payload) | 교체 가능 — 단, Stage 1 이 검증한 것만 실행됨 |

Stage 1 은 작고 (24 KB) 변하지 않으며, 오직 "Stage 2 가 진짜인지" 검증하는 것만 책임집니다.
검증을 통과한 Stage 2 만 실행되므로, 신뢰가 Stage 1 → Stage 2 로 전파됩니다 (chain of trust).

## Boot Flow

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
│ Stage 1 — Immutable Verifier                                │
│   1. Reset_Handler → clock/UART/LED bring-up                │
│   2. Smoke selftest (SHA / bignum 핵심 함수 sanity)          │
│   3. Stage2_Verify() — 6단계 trust chain (별도 문서)         │
│   4-a. PASS → SCB_VTOR = 0x60008000, MSP 재설정, 점프         │
│   4-b. FAIL → halt (LED solid, no recovery)                 │
└────────────────────────────────────────────────────────────┘
                          │ verify pass
                          ▼
┌────────────────────────────────────────────────────────────┐
│ Stage 2 — Verified Payload                                  │
│   - 자신의 vector table 로 재초기화                          │
│   - application 실행                                         │
└────────────────────────────────────────────────────────────┘
```

## Memory Map (FlexSPI NOR Flash, XIP)

| 영역 | 주소 범위 | 크기 | 내용 |
|---|---|---|---|
| Boot header | `0x60000000` ~ `0x60001FFF` | 8 KB | FlexSPI config, IVT, boot data |
| **Stage 1** | `0x60002000` ~ `0x60007FFF` | 24 KB | immutable verifier |
| **Stage 2** | `0x60008000` ~ `0x60047FFF` | 256 KB | verified payload + signature |
| **Metadata** | `0x600C8000` ~ `0x600C8FFF` | 4 KB | Rollback metadata — `[0x00]` `min_acceptable_version` (uint32 LE), 나머지 `0xFF` (C-5 에서 확장) |

RAM (실행 시):
| 영역 | 용도 |
|---|---|
| DTCM / OCRAM (`0x2xxxxxxx`) | stack, .data, .bss |
| modexp 작업 그릇 | `bn_t` (256 B), `bn2_t` (512 B) on stack |

## Stage 2 Image Layout

```
┌─────────────────────────────────────┬──────────────────────┐
│ code (size bytes)                   │ signature (256 bytes) │
│                                     │  RSA-2048 BE          │
│  offset 0x1C: magic = 0xDEADBEEF    │                       │
│  offset 0x20: size (code 길이)      │                       │
│  offset 0x24: CRC32 (of code)       │                       │
│  rest:        application code      │                       │
└─────────────────────────────────────┴──────────────────────┘
↑ 0x60008000           ↑ base+size        ↑ base+size+256
```

### 설계 결정

1. **헤더는 vector table 의 reserved slot 활용** (`0x1C`, `0x20`, `0x24`).
   별도 헤더 영역 없이 Cortex-M7 vector table 의 빈 자리를 재사용.

2. **signature 는 image 끝에 append** (256 byte).
   - vector reserved 자리는 64 byte 뿐이라 256 byte signature 못 들어감
   - image 시작은 vector table 이어야 함 (entry point 규칙)
   - 끝 append 가 공간 무제한 + code 영역과 분리

3. **size / CRC32 / SHA-256 영역은 모두 code 만** (signature 제외).
   signature 는 SHA-256(code) 의 결과이므로, SHA 영역이 signature 를 포함하면
   순환 의존 (chicken-and-egg) 발생. 따라서 signature 는 검증 영역의 외부 부속.

## Immutability 의 현재 한계

이 프로젝트에서 Stage 1 의 "immutable" 은 **관례** (재flash 안 함) 일 뿐,
hardware 적으로 강제되지 않습니다.

진정한 immutability 는:
- **eFuse SRK** (HAB 의 Super Root Key hash) — 한 번 구우면 비가역
- **ROM mask** — 칩 제조 시 고정

이 프로젝트는 chain of trust 의 **소프트웨어 부분** 을 학습 목적으로 구현한 것이며,
hardware root of trust (HAB) 는 의도적으로 범위에서 제외했습니다 (보드 1대 학습 환경에서
eFuse 굽기는 비가역 → hard brick 위험).

→ 자세한 trust chain 은 [Trust Chain](TRUST_CHAIN.md) 참조.
