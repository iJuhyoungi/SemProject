# RTWDOG 정리 (i.MX RT1020 RM Chapter 53)

> P-11 산출물. 여섯 번째 peripheral. 기능안전(functional safety)의 핵심 — "소프트웨어가 살아있는지 감시하는 죽은-사람-스위치(deadman switch)".
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 53 RTWDOG (WDOG3).

## PART 1 — 워치독이 무엇이고 왜 다른가
- 지금까지(SPI/ADC/PWM/CAN/GPT)는 무언가를 **하는** 기계. 워치독은 **CPU 가 멀쩡한지 감시하다 멈추면 강제로 리셋**하는 안전장치.
- 동작 원리: 카운터가 0 부터 계속 증가 → **TOVAL(timeout) 에 도달하면 칩을 리셋.** SW 가 정상이면 그 전에 주기적으로 **refresh(counter 0 으로)** 해서 리셋을 막는다.
- 즉 "SW 가 주기적으로 쓰다듬어 주지 않으면 = SW 가 죽었다고 판단 → 리셋". 무한 루프·데드락·폭주에서 시스템을 자동 복구.
- AUTOSAR **Wdg** 드라이버 = 이 RTWDOG. 자동차 ECU 기능안전(ISO 26262)의 필수 요소. `Wdg_SetTriggerCondition`(refresh)·`Wdg_SetMode` 가 핵심 API.

## PART 2 — RTWDOG 만의 특징 세 가지
### ① 설정 비트가 write-once
- CS 의 EN·CLK·INT·WIN·PRES·CMD32EN·UPDATE 등은 **리셋 후 단 한 번만 쓸 수 있다.** 잘못된 코드가 워치독을 마음대로 끄지 못하게 하는 안전 설계.
- 한 번 쓴 뒤 다시 바꾸려면 **unlock 시퀀스**가 필요(아래 ③). 그래서 초기 설정 때 `CS[UPDATE]=1` 을 같이 켜둬야 이후 재설정이 가능.

### ② Window mode — "너무 일찍 쓰다듬어도" 리셋
- 보통 워치독은 "늦게 refresh = 리셋". RTWDOG 은 옵션으로 **너무 빨리 refresh 해도 리셋**(CS[WIN]).
- 카운터가 WIN 값을 지나기 전에 refresh 하면 리셋. SW 가 예상보다 너무 빨리 루프를 돈다 = 그것도 이상신호로 간주. (P-11 기본 데모는 window 끔.)

### ③ 매직 시퀀스 (CNT 에 write)
- CNT 는 일반 카운터 쓰기가 아니라 **명령 레지스터**. 정해진 매직값을 순서대로 써야 동작 (오작동 방지).
- **Refresh**: `CMD32EN=1` 이면 32-bit 한 번 `0xB480_A602`. (아니면 16-bit 두 번 `0xA602` → `0xB480`)
- **Unlock**: `0xC520` → `0xD928` 을 16 bus clock 안에 연속 write. 이후 **128 bus clock 안에** 재설정 완료해야 함. (RM §53.3.2.2)
- 주의: refresh 두 write 사이에 인터럽트가 끼면 시퀀스가 무효가 될 수 있어, **refresh 동안 글로벌 인터럽트 잠깐 끄기** 권장 (RM).

## base / 클럭 / 리셋원인
- **RTWDOG base: `0x400B_C000`** (= WDOG3)
- 클럭 게이트: **CCM_CCGR5 CG2 (bits 5:4)** = wdog3 clock
- 카운터 클럭 소스(CS[CLK 9:8]): 1MHz RC OSC / 32kHz / IP Bus Clock 중. (리셋 기본 = 01)
- **리셋 원인 확인**: `SRC_SRSR`(SRC, base 0x400F_8000) 에 **wdog3_rst_b** 플래그. 부팅 시 이 비트를 읽으면 "직전 리셋이 워치독 때문이었는지" 판별 가능 → **데모 무한 리셋 루프 방지의 핵심.**

## 레지스터 맵 (RTWDOG, base 0x400B_C000)

| off | 이름 | 리셋값 | 역할 / 핵심 비트 |
|---|---|---|---|
| 0x00 | **CS** | 0x0000_2980 | STOP[0]/WAIT[1]/DBG[2]/TST[4:3]/**UPDATE[5]**/**INT[6]**/**EN[7]**/**CLK[9:8]**/RCS[10]RO/ULK[11]RO/PRES[12]/**CMD32EN[13]**/FLG[14]W1C/**WIN[15]** |
| 0x04 | **CNT** | 0x0000_0000 | refresh/unlock **명령**을 쓰는 레지스터 (매직값) |
| 0x08 | **TOVAL** | 0x0000_0400 | timeout 값 — 카운터가 여기 도달하면 리셋 |
| 0x0C | **WIN** | 0x0000_0000 | window mode 하한 (CS[WIN]=1 일 때만 의미) |

- 리셋 기본값 0x2980 = **EN[7]+CLK01[8]+ULK[11]+CMD32EN[13]** → RTWDOG 은 리셋 직후 이미 enable 상태(TOVAL=0x400)일 수 있음. 우리 boot/ROM 상태를 CS read 로 먼저 확인할 것.

## init / 데모 시퀀스 (안전 설계)
```
[부팅 시]
1) SRC_SRSR 의 wdog3_rst_b 읽기
   - set 이면: "직전에 워치독 리셋됨" → 플래그 W1C 클리어 + UART 출력, 데모 재무장 안 함(루프 방지)
   - clear 면: 정상 부팅 → 아래 설정 진행
2) 클럭 게이트   CCGR5 |= (3<<4)            # CG2 wdog3
3) (필요시) unlock  CNT=0xC520; CNT=0xD928   # write-once 비트 재설정 시
4) 설정         CS = EN | UPDATE | CLK | (CMD32EN) ...   # 128 bus clock 안에
                TOVAL = 타임아웃 (예: 32kHz 기준 몇 초)
5) RCS=1 확인   # 새 설정 반영 성공

[정상 동작 — Phase 1]
6) 메인 루프마다 Wdg1_Refresh()  # 0xB480_A602 → 보드 계속 살아있음, beat 출력 지속

[의도적 타임아웃 — Phase 2]
7) N beat 후 refresh 중단 → 카운터가 TOVAL 도달 → 칩 리셋 → UART 배너 재출력(관측)
   → 부팅 시 1) 에서 wdog3_rst_b 감지 → 한 번만 시연하고 멈춤
```

## 검증 (도구 없이)
- **Phase 1 (살아있음)**: refresh 하는 동안 beat 카운터가 끊기지 않고 계속 증가 → 워치독이 돌지만 정상적으로 먹이고 있음.
- **Phase 2 (리셋 실증)**: refresh 멈추면 일정 시간 뒤 **UART 배너가 처음부터 다시 출력** = 칩이 워치독에 의해 리셋됨. + 재부팅 직후 `SRC_SRSR[wdog3_rst_b]=1` 로 "이 리셋은 워치독이 일으킨 것"을 코드로 확인.
- 핵심 안전장치: **부팅 시 리셋 원인을 확인해 재무장하지 않음** → 무한 리셋 루프에 빠지지 않고 한 번만 시연.

## 계획
- P-11a: 개념+지도 ✅ (지금) → 본 문서
- P-11b: 클럭 게이트 → (unlock) → CS/TOVAL 설정 → refresh 루프(Phase1) → 의도적 타임아웃 리셋(Phase2) + SRC_SRSR 로 루프 방지
- (P-11c 선택) AUTOSAR Wdg API (Wdg_Init / Wdg_SetTriggerCondition / Wdg_SetMode) — Phase 2 에서

## AUTOSAR / 기능안전 연결
- AUTOSAR **Wdg** 드라이버 = 이 RTWDOG. 상위 WdgM(Watchdog Manager)이 주기 task 의 alive 를 감시하고, 정상일 때만 Wdg 를 refresh. ISO 26262 기능안전의 기본 빌딩블록 — 자동차 ECU narrative 에 강력.
