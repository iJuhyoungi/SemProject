# Fault Injection 하드닝

전압이나 클럭, 전자기 펄스로 CPU 를 순간적으로 흔들어 명령 하나를 건너뛰거나 값 하나를
망가뜨리는 공격을 **fault injection (글리치)** 이라고 부릅니다. 이 문서는 그 공격을
소프트웨어로 모사해 **이 프로젝트가 실제로 뚫린다는 것을 먼저 보이고**, 방어를 넣은 뒤
같은 공격이 막히는 것까지 보드에서 확인한 기록입니다.

[Threat Model](THREAT_MODEL.md) 의 위협 T-10 ~ T-15 에 대응하는 작업입니다.

## 왜 이 작업이 필요했는가

이 프로젝트의 암호 구현은 견고합니다. PKCS#1 v1.5 패딩을 전 구간 검사하고 DigestInfo
OID 까지 확인하며, SHA-256 도 정확합니다. 그런데 하드닝 전 상태에서 **서명이 1비트
변조된 이미지가 그대로 부팅됐습니다.**

```
[Verify] RSA signature FAIL          ← 암호 검증은 정확히 실패를 판정했는데
[BL2] App B OK - jumping
[App B] running (slot B)             ← 그 이미지가 실행됩니다
```

RSA-2048 을 수학적으로 깬 게 아닙니다. **검증의 결론을 실어 나르는 값 하나, 분기 하나**를
건드렸을 뿐입니다. 실제 상용 부트로더 공격 사례 대부분이 여기에 해당합니다. 암호를 아무리
정확하게 구현해도, 그 결론이 실행 결정으로 이어지는 경로가 부드러우면 전부 무의미합니다.

## 공격 모델

[Threat Model](THREAT_MODEL.md) 의 **AT-3 (fault injection 공격자)** 를 전제합니다.

- 보드를 물리적으로 확보하고 글리치 장비를 갖췄습니다.
- **한 번의 부팅에 성공하는 글리치는 1회**로 가정합니다. 다회 글리치는 정밀도 요구가
  급격히 올라가 비용 대비 난이도가 비대칭적으로 높습니다. 단일 글리치 내성을 먼저
  확보하는 것이 실무 관례입니다.
- private key 는 없습니다.

물리 글리치가 Cortex-M7 에 만들어내는 결과는 현실적으로 두 가지로 좁혀집니다.

| 모드 | 효과 | 대응 위협 |
|---|---|---|
| **값 손상** | 레지스터나 버스 위의 값에서 비트 하나가 뒤집힙니다 | T-12 |
| **명령 스킵** | 명령 하나가 실행되지 않습니다. 판정 분기가 대상이면 조건과 무관하게 진행됩니다 | T-10, T-11 |

## 하네스 설계 — 가장 중요한 결정

모사 하네스(`shared/include/glitch.h`)를 만들 때 가장 쉬운 구현은 이것입니다.

```c
#ifdef GLITCH_SIM
return 1;   /* 검증 성공으로 강제 */
#endif
```

**이건 공격 모사가 아니라 백도어입니다.** 어떤 방어를 넣어도 뚫리므로, 하드닝 후에
"방어가 통했다"를 보일 수가 없습니다. 하네스가 방어보다 강하면 실험 자체가 성립하지
않습니다.

그래서 **실제 글리치가 할 수 있는 만큼만** 흉내내도록 만들었습니다.

```c
/* ① 값 1비트 손상 */
static inline uint32_t glitch_bitflip(uint32_t v)
{
    UART1_SendString("[Glitch] single-bit fault injected on verdict\r\n");
    return v ^ GLITCH_BITFLIP_MASK;    /* 기본 마스크 = bit 0 */
}

/* ② 판정 분기 스킵 */
#define GLITCH_SKIP_BRANCH_TAKEN() \
    (UART1_SendString("[Glitch] verdict branch skipped\r\n"), 1)
```

이렇게 두면 **하드닝 후 같은 하네스가 실패하는 것 자체가 방어 효과의 증거**가 됩니다.
①은 성공 토큰을 멀리 떨어뜨리면 비트 하나로는 도달할 수 없어지고, ②는 판정을 서로 다른
두 지점에서 하면 글리치 1회로 둘 다 넘을 수 없습니다. 공격 모델과 방어가 정확히
대응합니다.

두 매크로 중 아무것도 정의되지 않으면 주입 지점이 전처리 단계에서 사라져 release 빌드에는
흔적이 남지 않습니다. 시뮬레이션 빌드는 부팅 시 배너를 찍어 실수로 남는 것을 막습니다.

```
[!!] GLITCH SIMULATION BUILD - NOT FOR RELEASE
```

주입 지점은 **Stage 2 → App 계층에만** 두었습니다. Stage 1 이 halt 하면 복구가 flash
재기록뿐이라 실험 비용이 크고, App 계층은 코드는 멀쩡히 두고 서명만 깨면 "검증에 실패해야
정상인데 부팅되어 버리는" 그림이 UART 로 선명하게 보이는 데다 실패해도 A/B fallback 이
받쳐줍니다.

## 재현 절차

```bash
# 1. 정상 빌드 후 전체 플래시
./build.sh && ./flash_mcu.sh all
python3 tools/set_metadata.py --seq 1 --min-version 1 && ./flash_mcu.sh metadata

# 2. App B 의 서명 마지막 바이트에서 bit 0 만 뒤집습니다 (코드 영역은 그대로)
#    빌드 후에 손상시켜야 합니다. build.sh 는 매번 다시 서명합니다.
python3 -c "p='build/app/app_b/app_b.bin'; d=bytearray(open(p,'rb').read()); d[-1]^=1; open(p,'wb').write(d)"
./flash_mcu.sh app_b

# 3-a. 글리치 ① — 값 1비트 손상
./build.sh -DGLITCH_SIM_BITFLIP=ON && ./flash_mcu.sh stage2

# 3-b. 글리치 ② — 판정 분기 스킵
./build.sh -DGLITCH_SIM_SKIP=ON && ./flash_mcu.sh stage2

# 4. 원상 복구
./build.sh && ./flash_mcu.sh stage2 && ./flash_mcu.sh app_b
```

App B 는 version 2 라 primary 로 먼저 시도됩니다. 손상된 서명이므로 정상 동작이라면
거부되고 App A(version 1)로 fallback 해야 합니다.

## Before / After

### 글리치 ① — 값 1비트 손상

**Before (하드닝 전)** — 뚫립니다.

```
[BL2] Trying App B ...
[Verify] SHA-256: d050172db339f2aaa53a7105fb5d19effeaf7d97bba7f8bb9398a52c8a5d3b37
[Verify] RSA signature FAIL                        ← 검증은 정확히 실패
[Glitch] single-bit fault injected on verdict      ← 비트 하나가 뒤집히고
[BL2] App B OK - jumping                           ← 성공으로 판정되어
[App B] running (slot B)                           ← 서명이 깨진 이미지가 부팅
```

`verify_image()` 가 성공에 `1`, 실패에 `0` 을 돌려주던 시절입니다. 두 값의 hamming
distance 가 **1** 이라 글리치 한 방이 정확히 그만큼을 해냅니다. 확률적 공격이 아니라
사실상 결정적인 공격입니다.

**After (하드닝 후)** — 막힙니다.

```
[BL2] Trying App B ...
[Verify] SHA-256: d050172db339f2aaa53a7105fb5d19effeaf7d97bba7f8bb9398a52c8a5d3b37
[Verify] RSA signature FAIL
[Verify] SHA-256: d050172db339f2aaa53a7105fb5d19effeaf7d97bba7f8bb9398a52c8a5d3b37
[Verify] RSA signature FAIL                        ← 독립 2회 실행, 둘 다 실패
[Glitch] single-bit fault injected on verdict
[BL2] verdict mismatch between two runs - rejecting ← 두 결과의 불일치로 거부
[BL2] Falling back to App A ...
[Verify] RSA signature OK
[BL2] App A OK - jumping
[App A] running (slot A)
```

### 글리치 ② — 판정 분기 스킵

**Before (하드닝 전)** — 뚫립니다.

```
[BL2] Trying App B ...
[Verify] RSA signature FAIL
[Glitch] verdict branch skipped                    ← 판정 분기가 실행되지 않고
[BL2] App B OK - jumping
[App B] running (slot B)
```

**중간 상태 (토큰만 적용, 판정은 아직 한 곳)** — 여전히 뚫립니다.

토큰을 도입한 직후 ①은 막혔지만 ②는 그대로 통과했습니다. **값의 강도로는 분기 스킵을
막을 수 없다**는 것을 보여주는 대목이고, 방어와 공격이 1:1 로 대응한다는 증거이기도
합니다. 이 결과가 판정 이중화의 근거가 되었습니다.

**After (하드닝 후)** — 막힙니다.

```
[BL2] Trying App B ...
[Verify] RSA signature FAIL
[Verify] RSA signature FAIL
[Glitch] verdict branch skipped
[BL2] App B OK - jumping                           ← 호출부 분기는 실제로 뚫렸는데
[Jump] verdict is not PASS - refusing to jump      ← 점프 직전 재판정이 차단
[BL2] Falling back to App A ...
[BL2] App A OK - jumping
[App A] running (slot A)
```

`App B OK - jumping` 이 출력된 **뒤에** 거부되는 것이 핵심입니다. 첫 방어선은 실제로
무너졌지만 두 번째가 잡아냈습니다. 다층 방어(defense in depth)가 로그 한 화면에 그대로
남은 셈입니다.

### 글리치 없는 기준선

방어가 정상 동작을 해치지 않는지도 함께 확인했습니다.

```
[BL2] Trying App B ...
[Verify] RSA signature FAIL
[Verify] RSA signature FAIL
[BL2] Falling back to App A ...       ← 글리치 없이도 변조 이미지는 거부
[App A] running (slot A)
```

## 적용한 방어

| # | 방어 | 위치 | 막는 위협 |
|---|---|---|---|
| 1 | **Hamming-distant 판정 토큰** — `SEC_PASS = 0xA5C33C5A` / `SEC_FAIL = 0x5A3CC3A5` | `shared/include/secure.h:7-8` | T-12 |
| 2 | **이중 조건 판정** — PASS 와 같은지에 더해 FAIL 과 다른지까지 확인 | `secure.h:12-17` | T-12 |
| 3 | **기본값 FAIL** — 결과 변수를 실패로 초기화하고 성공 대입 지점을 함수당 한 곳만 | `verify.c:41,96` / `rsa.c:18,105` | T-11 |
| 4 | **단계 카운터 사후 대조** — 통과한 검사 수를 마지막에 확인 | `verify.c:43,91` / `rsa.c:19,100` | T-11 |
| 5 | **루프 반복 횟수 검증** — PS 패딩 202바이트 루프가 실제로 돌았는지 확인 | `rsa.c:57,66` | T-13 |
| 6 | **상수시간 비교** — DigestInfo prefix 와 해시 비교 | `rsa.c:81,91` | T-14 |
| 7 | **독립 2회 실행 + 일치 확인** | `stage1/main.c:160` / `stage2/main.c:165,199` | T-10 |
| 8 | **점프 직전 재판정** — `jump_to_image()` 가 verdict 를 인자로 받아 스스로 확인 | `verify.c:102-106` | T-10 |
| 9 | **판정과 진단의 역할 분리** — 메타데이터 검증이 토큰을 반환하고 reason 은 출력 전용 | `metadata.c` | T-15 |

### 토큰 값을 고른 기준

```
SEC_PASS = 0xA5C33C5A = 1010 0101 1100 0011 0011 1100 0101 1010
SEC_FAIL = 0x5A3CC3A5 = 0101 1010 0011 1100 1100 0011 1010 0101
```

- `hamming(PASS, FAIL) = 32` — 글리치 1회로 넘을 수 없습니다
- `hamming(PASS, 0x00000000) = 16` — 레지스터가 통째로 0 이 되어도 PASS 가 아닙니다
- `hamming(PASS, 0xFFFFFFFF) = 16` — erase 된 flash 를 읽어도 PASS 가 아닙니다
- 각 토큰의 1 비트 개수가 16 — 특정 방향의 손상에 편향되지 않습니다

두 번째와 세 번째 조건이 놓치기 쉽습니다. 초기화되지 않은 메모리, erase 된 flash, 클리어된
레지스터에서 나오는 값이 `0x00000000` 과 `0xFFFFFFFF` 이므로, 성공 토큰이 이 근처면
"사고로 성공"이 생깁니다.

### 컴파일러가 방어를 지우지 못하게

단계 카운터와 루프 카운터에는 `volatile` 이 필수입니다.

```c
volatile uint32_t steps = 0u;
```

없으면 `-O2` 가 "steps 는 항상 6이니 비교는 참"이라고 상수 접기를 해서 대조 자체를
삭제합니다. 컴파일러에게는 죽은 코드이지만 우리에게는 방어 코드입니다.

## 검증

호스트 단위 테스트 **47건**이 통과합니다 (bignum 26 / sha256 6 / rsa 5 / secure 10).

`secure` 스위트에는 하드닝의 핵심 성질을 자동으로 확인하는 케이스가 들어 있습니다.

- 두 토큰의 hamming distance 가 32 인지
- 0 과 0xFFFFFFFF 로부터 충분히 떨어져 있는지
- **어떤 비트 하나를 뒤집어도 PASS 판정이 나오지 않는지** (32가지 비트 위치 전부)
- 상수시간 비교가 256가지 바이트 차이 전 구간에서 정확한지

보드 검증은 이 문서의 Before/After 절이 그대로 결과입니다.

## 이 하드닝이 막지 못하는 것

| 남는 위협 | 이유 |
|---|---|
| **다중 글리치** (한 부팅에 2회 이상) | 방어를 3겹으로 두었으므로 3회 이상의 정밀한 글리치가 필요합니다. 공격 난이도가 비대칭적으로 올라가 단일 글리치 내성까지를 목표로 했습니다. |
| **T-19 TOCTOU** | XIP 구조라 검증 대상과 실행 대상이 같은 flash 입니다. 검증과 점프 사이에 flash 내용을 바꾸는 공격은 이론상 가능하며, 물리 공격자와 정밀한 타이밍이 동시에 필요해 잔여 위험으로 수용했습니다. |
| **T-05 / T-08 / T-09** | 메타데이터 replay 와 Stage 1 불변성은 소프트웨어로 닫을 수 없습니다. 하드웨어 root of trust 와 OTP monotonic 카운터가 필요합니다. |

## 비용

검증을 두 번 독립 실행하므로 RSA-2048 검증 횟수가 Stage 1 에서 2회, Stage 2 에서
2~4회로 늘어납니다. 부팅 시간이 대략 두 배가 됩니다. 부팅 때 한 번 내는 비용이라
실무에서도 흔히 받아들이는 트레이드오프입니다. 더 줄여야 한다면 전체 검증 대신 서명
검증 단계만 재실행하는 방식도 가능합니다.

## 작업 중 겪은 것 — 토큰 도입의 함정

토큰을 도입한 뒤 `verify.c` 에 예전 판정이 남아 있었습니다.

```c
if (!rsa_verify_pkcs1_v15_sha256(img_hash, signature, modulus))
```

`SEC_FAIL` 은 0 이 아니므로 `!SEC_FAIL` 은 **항상 거짓**입니다. 실패 분기에 절대
진입하지 않아 **RSA 서명 검증이 통째로 무력화된 상태**였습니다. 어떤 서명이든 통과하는
fail-open 입니다.

방어의 근거였던 "SEC_FAIL 은 0 이 아니다"라는 성질이 그대로 함정이 됩니다. 반환 타입을
토큰으로 바꿀 때는 **호출부를 전수 확인해야 합니다.**

```bash
grep -rnE "\(![[:space:]]*(vector_sane|verify_image|rsa_verify_pkcs1_v15_sha256|metadata_read_active)[[:space:]]*\(" shared bootloader app
```

테스트 코드도 같은 함정을 갖습니다. `TEST_ASSERT_TRUE(SEC_FAIL)` 은 조용히 통과하므로,
`TEST_ASSERT_EQUAL_HEX32(SEC_PASS, ...)` 처럼 **정확한 값**을 비교해야 합니다.

이 문제는 "서명이 변조된 이미지가 글리치 없이도 거부되는가"를 확인하는 기준선 테스트에서
잡혔습니다. 방어를 넣는 실험에서 **방어가 없어도 통과해야 할 케이스**를 함께 돌리는 것이
왜 필요한지 보여주는 사례입니다.

---

관련 문서입니다. 공격자 모델과 위협 목록은 [Threat Model](THREAT_MODEL.md), 검증 단계의
상세는 [Trust Chain](TRUST_CHAIN.md), 전체 구조는 [Architecture](ARCHITECTURE.md) 를
참고해 주세요.
