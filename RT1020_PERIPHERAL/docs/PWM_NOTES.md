# eFlexPWM 정리 (i.MX RT1020 RM Chapter 50)

> P-8 산출물. SPI·ADC 에 이은 세 번째 peripheral. 타이머 기반 출력이라 결이 다름(주파수·듀티 사이클).
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 50 FlexPWM.

## 같은 패턴 + 새 개념
- 패턴은 동일: 클럭(CCM) → 핀(IOMUXC) → 설정 → enable → 사용.
- 새 개념: **카운터 + 비교 매칭으로 파형 생성.** 서브모듈(SM0~3)마다 독립 카운터.
  - 카운터가 `INIT` → `VAL1`(주기 top) 를 돌며 반복.
  - `PWM_A` 출력: 카운터가 `VAL2` 도달 시 HIGH, `VAL3` 도달 시 LOW → (VAL3-VAL2)/주기 = 듀티.
  - 주기 = (VAL1 - INIT + 1) × prescaler / PWM 클럭.

## base / 클럭
- FLEXPWM1: `0x403D_C000` (16KB). 서브모듈 4개(SM0~SM3) + 모듈레벨 레지스터.
- 클럭 게이트: **CCGR4[CG8]** (FLEXPWM1), CCGR4[CG9] (FLEXPWM2)

## 레지스터 (FLEXPWM1, base 0x403D_C000)
### 서브모듈0 (16-bit)
| off | 이름 | 역할 |
|---|---|---|
| 0x00 | SM0CNT | 현재 카운터 값 (RO, 동작 확인용) |
| 0x02 | SM0INIT | 카운터 초기값 (보통 0) |
| 0x06 | **SM0CTRL** | PRSC[6:4] prescaler, FULL[10] full-cycle reload(버퍼 로드 위해 필요) |
| 0x0E | **SM0VAL1** | 주기 (카운터 top = modulo) |
| 0x12 | **SM0VAL2** | PWM_A rising edge (보통 0) |
| 0x16 | **SM0VAL3** | PWM_A falling edge (= 듀티) |
| 0x1A/0x1E | SM0VAL4/5 | PWM_B edge |
| 0x30/0x32 | SM0DTCNT0/1 | deadtime (상보 출력용, 지금 미사용) |

### 모듈레벨
| off | 이름 | 역할 |
|---|---|---|
| 0x180 | **OUTEN** | 출력 enable. PWMA_EN[11:8] (SM3~0). **SM0 PWM_A = bit8** |
| 0x188 | **MCTRL** | LDOK[3:0](버퍼 로드, SM0=bit0), RUN[11:8](클럭 시작, SM0=bit8), CLDOK |
| 0x18C/0x18E | FCTRL0/FSTS0 | fault (미사용) |

## init 시퀀스
```
1) 클럭     CCGR4[CG8] = 11 (FLEXPWM1 게이트)
2) 핀       IOMUXC: FLEXPWM1_PWMx_A 출력 ALT (P-8b 에서 정확한 pad 확정)
3) CTRL     SM0CTRL = PRSC(분주) | FULL(=1, full-cycle reload 켜야 VAL 버퍼 로드됨)
4) 값       SM0INIT=0, SM0VAL1=주기, SM0VAL2=0, SM0VAL3=듀티
5) 출력     OUTEN |= (1<<8)         # SM0 PWM_A 출력 enable
6) 로드+런  MCTRL |= (1<<0)         # LDOK SM0: 버퍼된 INIT/VAL 로드
            MCTRL |= (1<<8)         # RUN SM0: 서브모듈 클럭 시작 → 파형 생성
```
- **LDOK 주의**: INIT/VAL 레지스터는 더블 버퍼라, MCTRL[LDOK] 세팅해야 실제 반영. CTRL[FULL](또는 HALF) 중 하나가 켜져 있어야 reload 됨.

## 검증 (scope/LA 없이)
- PWM 출력은 **파형**이라 핀에서 직접 보려면 로직애널라이저/오실로스코프 필요 (SPI loopback 과 비슷한 제약).
- 무도구 검증: **SM0CNT 를 두 번 읽어 값이 진행/wrap 하면** 생성기 동작 확인. + 레지스터 readback(VAL1/주기).
- (가능하면) PWM 출력 핀이 보드 LED 와 연결되면 **저주파(1~2Hz) PWM 으로 LED 밝기/깜빡** 시연 — P-8b 에서 핀 가용성 확인.

## 계획
- P-8a: 정리 ✅ (지금)
- P-8b: 클럭+핀+CTRL+VAL+OUTEN+MCTRL → PWM 1채널 생성, CNT 변화로 검증
- (P-8c 선택): AUTOSAR Pwm API (Pwm_Init / Pwm_SetDutyCycle) 파사드

## AUTOSAR 연결
- AUTOSAR **Pwm** 드라이버 = 이 eFlexPWM. 모터·액추에이터 제어의 기본. 듀티/주파수 제어가 핵심 API.
