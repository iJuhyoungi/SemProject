# FlexCAN 정리 (i.MX RT1020 RM Chapter 40)

> P-9 산출물. 네 번째 peripheral, 지금까지 중 가장 복잡. 멀티노드 버스 프로토콜.
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 40 FLEXCAN.

## PART 1 — CAN 이 무엇이 다른가
- SPI/ADC/PWM 은 로컬·점대점. **CAN 은 멀티노드 버스** (여러 ECU 가 2가닥 선 공유, 자동차 표준).
- **프레임 단위** 통신: ID(우선순위 겸 식별자) + DLC(데이터 길이 0~8B) + 데이터 + CRC + ACK.
- **주소가 아니라 ID 기반**: 송신자가 ID 붙여 broadcast → 수신자는 ID 필터로 원하는 것만 수신.
- **비트 타이밍이 핵심**: 모든 노드가 같은 baud 로 동기화돼야 함. 한 비트 시간을 time quanta(Tq)로 쪼개 세그먼트로 구성.

## PART 2 — FlexCAN 구조
- 모듈 + **메시지 버퍼(MB) 배열** (각 MB = TX 또는 RX 슬롯, ID/DLC/데이터 보관). RT1020 최대 64 MB.
- MB 의 **CODE 필드**가 역할 결정:
  - `0xC`(1100) = TX, 데이터 프레임 1회 전송
  - `0x4`(0100) = RX EMPTY, 수신 대기 (프레임 들어오면 HW 가 `0x2` FULL 로 바꿈)
- 비트 타이밍은 CTRL1: PRESDIV(프리스케일), PROPSEG, PSEG1, PSEG2, RJW.

## PART 3 — loopback (부품 없이 검증!)
- **CTRL1[LPB]=1 → TX 가 내부적으로 RX 에 연결.** 트랜시버·버스 없이 **자기가 보낸 프레임을 자기가 수신.**
- → 보드 단독으로 **진짜 송수신 검증 가능** (SPI 는 loopback 못 했지만 CAN 은 HW loopback 내장).
- loopback 이라 **핀 muxing 불필요** (TX=AD_B0_04, RX=AD_B0_05 ALT1 이지만 안 씀).

## PART 4 — FlexCAN 특유: Freeze Mode
- **설정 변경은 반드시 Freeze Mode 에서** 해야 함 (CAN 버스와 비동기 상태로 안전하게 설정).
- 모듈 enable 하면 자동으로 Freeze 진입.

## init 시퀀스 (RM 40.8)
```
1) 모듈 enable     MCR[MDIS]=0
2) soft reset      MCR[SOFTRST]=1 → 0 될 때까지 폴링
3) Freeze 진입     MCR[FRZ]=1, MCR[HALT]=1 → MCR[FRZACK]=1 대기
4) 비트타이밍+LPB  CTRL1 = PRESDIV|PROPSEG|PSEG1|PSEG2|RJW | LPB(loopback)
5) MB 초기화       RX MB: CODE=EMPTY(0x4)+ID, 나머지 INACTIVE(0). MAXMB(MCR) 설정
6) Freeze 해제     MCR[HALT]=0 → MCR[NOTRDY]=0 대기 (동기화 완료)
7) 전송            TX MB 에 ID/DLC/데이터 쓰고 CODE=0xC → 전송
                   → loopback 으로 RX MB 가 받음 → RX MB 의 CODE=FULL 확인 후 읽기
```

## 비트 타이밍 개념 (제일 까다로운 부분)
- 한 비트 = Tq × (SYNC(1) + PROPSEG+1 + PSEG1+1 + PSEG2+1)
- Tq = (PRESDIV+1) / CAN_clock
- baud = CAN_clk / (PRESDIV+1) / (총 Tq 수)
- loopback 은 실제 버스 상대가 없어 정확한 baud 가 덜 중요 — 단 세그먼트 값은 유효 범위여야 함.

## 레지스터 맵 (FLEXCAN1, base 0x401D_0000)
| off | 이름 | 역할 |
|---|---|---|
| 0x00 | **MCR** | MDIS/SOFTRST/FRZ/HALT/FRZACK/NOTRDY/MAXMB. 모듈·freeze 제어 |
| 0x04 | **CTRL1** | 비트타이밍(PRESDIV[31:24]/RJW[23:22]/PSEG1[21:19]/PSEG2[18:16]/PROPSEG[2:0]) + **LPB[12]**(loopback)/LOM[3] |
| 0x80~ | **MB[n]** | 메시지 버퍼 (각 16B): word0=CODE/SRR/IDE/RTR/DLC, word1=ID, word2-3=데이터 |
| 0x880~ | RXIMR[n] | RX 개별 마스크 (ID 필터) |
- 클럭 게이트: **CCGR0[CG8]** (FLEXCAN1). CAN 클럭 소스는 CTRL1[CLKSRC].

## 계획
- P-9a: 개념+지도 ✅ (지금)
- P-9b: 모듈 enable + soft reset + freeze + 비트타이밍 + LPB + MB 초기화 (init 까지)
- P-9c: TX 프레임 1개 → loopback 수신 → RX MB 데이터 일치 확인

## AUTOSAR / 자동차 연결
- AUTOSAR **Can** 드라이버 = 이 FlexCAN. CAN 통신 스택(CanIf/PduR/Com)의 맨 아래. 자동차 ECU 통신의 핵심.
