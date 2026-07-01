# 트러블슈팅: 워치독 무한 리셋 — 알고 보니 I-cache/XIP 문제

> RT1020_PERIPHERAL, AUTOSAR 래핑(Phase 2) 중 발생. "무해한 코드 한 조각을 추가했더니 워치독이 무한 리셋"하는 Heisenbug를, 여러 오답을 거쳐 **I-cache 미활성 + flash XIP 실행**이라는 진짜 원인까지 추적한 기록.
> 결론부터: 워치독 드라이버는 멀쩡했고, 범인은 **캐시가 꺼진 채 flash에서 직접 실행되는 busy-delay 루프의 실행 시간이 코드 주소 정렬에 의존**한 것.

## 1. 증상
- SPI/ADC/PWM/CAN/GPT/Wdg/Icu 7개 드라이버가 도는 펌웨어에서, **GPT에 AUTOSAR facade(`Gpt_*`)를 추가한 순간부터** 부팅 직후 무한 재부팅.
- 시리얼: 초기화 로그는 다 정상으로 찍히는데 **메인 루프의 `[PERI] beat`가 한 줄도 안 나오고** 재부팅.
- 리셋 원인 레지스터 `SRC_SRSR = 0x80` (wdog3_rst_b) → **RTWDOG가 리셋을 일으킴.** HardFault 핸들러의 `[FAULT]`는 안 뜸 → 진짜 비동기 워치독 리셋(코드 예외 아님).

## 2. 왜 어려웠나 (Heisenbug의 성격)
- **코드 몇 줄을 넣고 빼는 것만으로 증상이 토글**됐다. Gpt facade를 되돌리면 정상, 다시 넣으면 리셋.
- 워치독 설정 되읽기는 **전부 정상**으로 보였다: `CS=0x25A0`(EN·CMD32EN·CLK=LPO·RCS=1), `TOVAL=0xFFFF`.
- `refresh`도 정상 동작했다(계측: CNT `0x52C → 0x4A`로 리셋됨).
- **빌드마다 초기 타이밍 측정값까지 달라졌다** — 워치독과 무관한 SPI 구간의 `free loops dma`가 `0x62A`(정상) vs `0x12`(깨짐). 이건 나중에 결정적 단서가 된다.

## 3. 틀린 가설들 (정직하게)
근본 원인에 닿기 전, 아래 가설들을 세우고 **하나씩 반증**했다:

| 가설 | 반증 |
|---|---|
| busy-delay가 워치독 타임아웃보다 김 (타이밍) | delay를 50M→20M로 줄여도 동일하게 리셋 |
| `LED_Off`(새로 호출됨)가 hang | 단순 GPIO 1비트 write, 멈출 수 없음 |
| RTWDOG 재설정 128-bus-clock 윈도우를 GPT IRQ가 늘림 | `cpsid/cpsie`로 감싸도 그대로 리셋 |
| 재설정 직후 첫 refresh 무효(prime 필요) | init에 prime 넣어도 그대로 |
| GPT 인터럽트가 직접 원인 | (부분적 함정 — 4절 참고) |

교훈: **변경 내용상 워치독이 깨질 논리가 없는데 깨진다** = 코드 로직이 아니라 **레이아웃/타이밍에 민감한 잠복 버그**를 의심해야 했다.

## 4. 결정적 전환 — 추측을 버리고 하드웨어 계측
직렬 추측이 계속 빗나가자 방법을 바꿨다.

### (1) git bisect로 트리거 확정
커밋된 P-12(워치독 정상)를 `git stash`로 되돌려 확인 → 정상. 즉 **환경이 아니라 uncommitted diff가 범인**. Gpt facade만 되돌리니 정상 → **Gpt facade 추가가 트리거** 확정. (단, facade 코드 자체는 무해 — `Gpt_Init`은 `Gpt1_Init` 호출뿐.)

### (2) 보드에 직접 계측
`build.sh` + `flash_mcu.sh`(pyOCD) + `cat /dev/ttyACM0`로 **빌드→플래시→시리얼 캡처를 직접** 돌리며, 메인 루프 각 단계에서 워치독 CNT를 찍었다:

```
[T ]CNT=0x00F1   [TR]CNT=0x14   ← top refresh 정상(→0)
[D1]CNT=0x1071   [MR]CNT=0x14   ← 첫 delay 후 0x1071, mid refresh 정상(→0)
<리셋 — [D2] 안 찍힘>
```

**핵심 발견 두 가지:**
- CNT 최대값이 `0x1071`(4209)뿐 — `TOVAL=0xFFFF`(65535)의 **1/16**. 카운터 오버플로우가 **아니다**. 그런데 SRSR=0x80.
- 리셋은 **둘째 delay(`[MR]` 다음)에서만** 발생. 첫째 delay는 항상 완주(`[D1]` 찍힘). **똑같은 `delay_busy(20000000)`인데 둘째만 안 끝난다.**

→ "동일한 코드인 둘째 delay만 완주 못 한다"가 진짜 단서였다. 첫째 delay(0.13초)와 달리 둘째 delay는 **>2초** 걸려 그 사이 카운터가 0xFFFF에 도달(우리가 CNT를 못 본 이유 = 리셋이 D2 출력 전에 발생).

### (함정) GPT IRQ 실험의 오답
GPT IRQ(`NVIC_ISER3`)를 끄니 리셋이 사라졌다 → "GPT IRQ가 원인"이라 오판했다. 사실은 **그 한 줄을 지우며 코드가 밀려 delay 루프 정렬이 바뀐** 것이 진짜 이유였다(아래). 레이아웃 민감 버그의 전형적 함정.

## 5. 근본 원인
정적 분석과 하드웨어 계측을 종합해 확정:

### 주범 — I-cache OFF + flash XIP
- 이 펌웨어는 `IS_BOOTLOADER` 분기라 `.text`를 RAM에 복사하지 않고 **flash에서 직접 실행(XIP)**한다.
- 그런데 **I-cache가 한 번도 켜지지 않았다**(`SCB_CCR.IC` 미설정). 즉 모든 명령어를 매번 FlexSPI로 fetch.
- `delay_busy(20000000)`는 `while(n--) nop;` 짧은 back-branch 루프. **캐시 없이 XIP면, 이 루프가 flash fetch-line(32B) 경계에 걸치는지에 따라 실행 속도가 수 배 달라진다.**
- Gpt facade가 링크되며 `main`과 그 안의 delay 루프 주소가 밀렸고, **둘째 delay 루프가 fetch-line 경계를 straddle** → 매 iteration마다 flash 재요청 → **>2초** → 워치독 타임아웃.
- 빌드마다 초기 측정값(`free loops` 등)이 달라진 것도 전부 이 XIP 실행속도 차이였다(캐시가 있으면 빌드 간 거의 동일했을 값).

### 부수 버그 — `cpsid i` 오타
- `Wdg1_Init`에서 재설정 구간을 인터럽트로부터 보호하려 `cpsid i`(disable)로 감싼 뒤, 끝에서 `cpsie i`(enable)로 복구해야 하는데 **또 `cpsid i`를 써서** 인터럽트가 계속 마스크됐다.
- 그 결과 GPT 알림(tick)이 죽어 있었고, 앞선 `cpsid/cpsie` 실험이 "효과 없던" 이유도 이것(이미 마스크 상태).

## 6. 수정

### I-cache 활성화 (`main` 시작)
```c
/* XIP(flash 직접 실행) 를 결정적/고속으로 — delay 루프 속도가 주소 정렬에 의존하던 문제 제거 */
*(volatile uint32_t *)0xE000EF50 = 0u;   /* ICIALLU: I-cache 전체 무효화 */
__asm volatile ("dsb"); __asm volatile ("isb");
SCB_CCR |= (1u << 17);                    /* CCR.IC = 1 : I-cache enable */
__asm volatile ("dsb"); __asm volatile ("isb");
```
> D-cache는 **일부러 안 켰다** — SPI DMA와의 coherency 관리가 별도로 필요해서다. 명령어 실행만 결정적이면 되므로 I-cache로 충분.

### 인터럽트 복구 오타 (`Wdg1_Init` 끝)
```c
-    __asm volatile ("cpsid i");
+    __asm volatile ("cpsie i");   /* 재설정 끝났으니 인터럽트 복구 */
```

## 7. 검증 (하드웨어)
수정 후 시리얼 캡처:

```
boot1 (SRSR=0x40, POR):   beat 0~4 refresh → 5부터 중단 → CNT 0x2182→0xEB66→0xFFFF 상승 → beat 0x0B 에서 리셋 (의도된 Phase 2)
boot2 (SRSR=0x80, 워치독): 영구 refresh → beat 0x24+ 계속 생존
[Gpt] tick = 0x17→0x18→0x19→0x1A   (인터럽트 복구 → tick 정상)
```
- **D1 = D2 = 0x1071 균일** — 캐시-off 시 둘째만 느렸던 비대칭이 완전히 사라짐 = 근본 원인 확정.
- Phase 1 생존 / Phase 2 타임아웃 리셋 / 재부팅 후 영구 생존 = 워치독 데모가 설계대로 동작.

## 8. 교훈
- **XIP + 캐시 OFF는 busy-loop 실행시간이 코드 레이아웃에 의존한다.** 워치독처럼 타이밍에 민감한 코드가 있으면 최소한 **I-cache는 켜야** 결정적으로 동작한다.
- **"코드 몇 줄로 증상이 토글되는 Heisenbug"** 는 로직 버그가 아니라 **레이아웃 민감(캐시/XIP/스택/정렬)** 을 먼저 의심.
- **추측을 반복하지 말고 하드웨어 데이터로.** 빌드→플래시→시리얼 계측을 자동화해(`build.sh`/`flash_mcu.sh`/`cat /dev/ttyACM0`) 각 지점의 레지스터 값을 찍으니, "둘째 delay만 안 끝난다"는 결정적 사실이 한 번에 드러났다.
- **워치독은 busy-loop 타이밍에 목매지 말 것.** 실무라면 리프레시를 (GPT tick 같은) 실제 시간 기준 ISR에서 하거나, 타임아웃 여유를 크게 두는 편이 견고하다.
- 확정 전엔 여러 그럴듯한 가설(타이밍/LED/IRQ/reconfig)이 다 틀릴 수 있다 — **반증 가능한 실험**으로 하나씩 지워야 한다.

## 부록 — 관련 파일
- 수정: `src/main.c`(I-cache enable), `src/Wdg.c`(cpsie), `src/Gpt.c`(NVIC 복원)
- 배경: `docs/WDG_NOTES.md`(RTWDOG), `docs/GPT_NOTES.md`(GPT), `docs/GLOSSARY.md`(약어)
