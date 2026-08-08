# SDRAM OTA A/B — i.MX RT1020 무중단 업데이트 부트로더

> 베어메탈 환경에서 **A/B 파티션 기반 무중단 펌웨어 업데이트**를 구현한 프로젝트입니다.
> 업데이트가 실패해도 이전 버전으로 자동 복귀하고, 부트로더 자체가 깨져도 UART 로
> 복구할 수 있습니다. 타깃은 Cortex-M7 기반 NXP i.MX RT1020 입니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![scheme](https://img.shields.io/badge/update-A%2FB_dual_slot-green)]()
[![recovery](https://img.shields.io/badge/recovery-UART_5--phase-orange)]()

---

## 한눈에

펌웨어 업데이트에서 가장 무서운 것은 **업데이트 도중 전원이 끊기는 것**입니다. 하나뿐인
슬롯에 덮어쓰는 방식이라면 그 순간 기기가 벽돌이 됩니다.

A/B 방식은 실행 중이 아닌 반대편 슬롯에 새 이미지를 쓰고, 다 쓴 뒤에 **포인터 하나만
바꿔서** 전환합니다. 쓰기가 실패해도 지금 돌고 있는 쪽은 그대로 남습니다.

여기에 두 겹의 안전망을 더했습니다.

- **롤백**: 새 이미지로 부팅했는데 앱이 "정상 기동"을 보고하지 않으면, 다음 부팅에서
  자동으로 이전 슬롯으로 되돌아갑니다.
- **Recovery 슬롯**: Stage 2 부트로더 자체가 깨져도 Stage 1 이 이를 감지해 recovery 로
  넘어가고, UART 로 새 Stage 2 를 받아 복구합니다.

---

## 메모리 맵 (FlexSPI NOR Flash, XIP)

| 영역 | 주소 | 크기 | 내용 |
|---|---|---|---|
| FCB | `0x60000000` | 4 KB | FlexSPI configuration block |
| IVT / boot data | `0x60001000` | 4 KB | 부팅 벡터 |
| **Stage 1** | `0x60002000` | 8 KB | 변경하지 않는 최소 부트로더. Stage 2 를 CRC32 로 검사 |
| **Stage 2** | `0x60004000` | 240 KB | A/B 정책 판단과 슬롯 전환 |
| **App Slot A** | `0x60040000` | — | 응용 슬롯 A |
| **App Slot B** | `0x60200000` | — | 응용 슬롯 B |
| **Recovery** | `0x603E0000` | 64 KB | UART 복구 부트로더 |
| **boot_ctrl** | `0x603F0000` | 4 KB | 부팅 제어 블록 (슬롯 상태) |

Stage 1 과 Stage 2 를 나눈 이유가 있습니다. **Stage 2 는 업데이트 대상이지만 Stage 1 은
아닙니다.** 업데이트가 건드리지 않는 최소한의 코드가 남아 있어야 "부트로더가 깨졌을 때
누가 구하는가" 에 답할 수 있습니다.

---

## 부팅 제어 블록

flash sector 하나에 상태를 둡니다.

```c
typedef struct {
    uint32_t magic;
    uint32_t active_slot;    /* 지금 정상으로 인정된 슬롯 */
    uint32_t pending_slot;   /* 시험 부팅 중인 슬롯 */
    uint32_t boot_success;   /* 앱이 정상 기동을 보고했는가 */
    uint32_t boot_attempts;  /* 시험 부팅 시도 횟수 */
} boot_ctrl_t;
```

`pending_slot` 과 `boot_success` 의 조합이 핵심입니다. 새 이미지는 곧바로 `active` 가
되지 않고 **시험 부팅(trial)** 상태로 들어갑니다. 앱이 스스로 `boot_success` 를 기록해야
비로소 확정되고, 그러지 못하면 `boot_attempts` 가 쌓이다가 이전 슬롯으로 되돌아갑니다.

**앱이 자신의 정상 동작을 스스로 증명해야 한다**는 것이 이 설계의 요점입니다. 부트로더는
앱이 잘 도는지 알 방법이 없으므로, 판단을 앱에게 넘기고 침묵을 실패로 해석합니다.

---

## 부팅 흐름

```
Stage 1
  └─ Stage 2 의 CRC32 검사
       ├─ OK   → Stage 2 로 점프
       └─ FAIL → Recovery 로 점프 (UART 대기)

Stage 2
  └─ boot_ctrl 읽기
       ├─ pending 시험 부팅 중인가?
       │    ├─ boot_success → 확정 (active = pending)
       │    └─ 시도 횟수 초과 → 이전 슬롯으로 롤백
       └─ 선택된 슬롯의 유효성 검사 → 점프

App (Slot A 또는 B)
  └─ 정상 기동을 확인한 뒤 boot_ctrl 에 boot_success 기록
```

---

## Recovery — UART 5단계 프로토콜

Stage 2 가 깨졌을 때 보드를 살리는 경로입니다. 호스트의
[`tools/uart_uploader.py`](tools/uart_uploader.py) 가 새 Stage 2 바이너리를 보냅니다.

| 단계 | 내용 | 신호 |
|---|---|---|
| 1 | Handshake | `RECV?` → `READY!` |
| 2 | 크기 협상 | `ACK` 또는 `TOOBIG` |
| 3 | 데이터 전송 | — |
| 4 | CRC 검증 | `CRCOK` 또는 `CRCFAIL` |
| 5 | Flash 기록 + 검증 | `DONE` |

**전송이 끝난 뒤 CRC 를 먼저 확인하고 그 다음에 flash 를 씁니다.** 순서가 반대라면
깨진 데이터로 유일한 복구 경로마저 덮어쓰게 됩니다.

상세는 [`docs/PROTOCOL_RECOVERY_UART.md`](docs/PROTOCOL_RECOVERY_UART.md) 에 있습니다.

---

## 사용법

```bash
./build.sh                  # Stage 1, Stage 2, Recovery, App A/B 빌드
./flash_mcu.sh              # pyOCD 로 플래시 (대상 인자 지원)

# Recovery 모드로 Stage 2 재전송
python3 tools/uart_uploader.py --port /dev/ttyACM0 --file build/.../stage2.bin
```

---

## 겪은 문제들

기록으로 남길 가치가 있었던 것들입니다. 상세는 `docs/` 에 있습니다.

**Stage 1 → Stage 2 점프 직후의 Hard Fault** — `cpsid i` 로 인터럽트만 막고 NVIC 의
enable 비트와 SysTick 은 그대로 둔 것이 원인이었습니다. Stage 2 가 `SystemInit` 에서
`cpsie i` 하는 순간 pending 되어 있던 IRQ 가 발생했고, 그 시점은 Stage 2 의 ramfunc 재배치가
끝나기 전이라 ITCM 의 죽은 영역으로 점프해 `CFSR.INVSTATE` 가 떴습니다. **점프 전에
SysTick 정지, NVIC ICER/ICPR 클리어까지 해야** 합니다.

**ROM API 로 flash 를 지울 때의 주소 기준** — AHB 주소(`0x60xxxxxx`)가 아니라 flash 기준
오프셋을 넘겨야 했습니다. 두 주소 공간을 섞어 쓰면 엉뚱한 sector 를 지웁니다.

**boot_ctrl 쓰기 도중의 원자성** — erase 후 program 사이에 전원이 끊기면 제어 블록이
모두 `0xFF` 가 됩니다. magic 검사로 이 상태를 감지해 안전한 기본 동작으로 떨어지게
했습니다.

---

## 다음 단계

이 프로젝트의 신뢰 사슬은 **CRC32 무결성**까지입니다. "손상되었는가"는 알 수 있지만
"누가 만들었는가"는 모릅니다. 여기에 서명 검증을 얹은 것이
[SDRAM_SECURE_BOOT](../SDRAM_SECURE_BOOT/) 입니다.

---

_학습 및 포트폴리오 목적입니다._
