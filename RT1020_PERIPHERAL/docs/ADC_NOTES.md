# ADC 지도 (i.MX RT1020 RM Chapter 61)

> P-6 산출물. SPI(LPSPI)에 이은 두 번째 peripheral. "클럭→핀→설정→enable→사용" 패턴이 ADC 에도 그대로 적용됨을 실증.
> 출처: NXP i.MX RT1020 RM Rev.2, Chapter 61 Analog-to-Digital Converter (ADC).

## 같은 패턴, 새 peripheral
- SPI 와 동일: 클럭(CCM) → 핀(IOMUXC) → 설정 → enable → 사용.
- ADC 추가 요소: ① **캘리브레이션**(정확도) ② **변환 완료 플래그 COCO** 폴링 ③ 결과 레지스터 R0 읽기.

## base / 클럭
- ADC1: `0x400C_4000`, ADC2: `0x400C_8000` (각 16KB)
- 클럭 게이트: **CCGR1[CG8]** (ADC1), CCGR1[CG4] (ADC2)
- ADC 내부 클럭(ADCK): `CFG[ADICLK]` 로 소스 선택(bus clock), `CFG[ADIV]` 로 분주

## 레지스터 (ADC1, base 0x400C_4000)
| off | 이름 | 역할 |
|---|---|---|
| 0x00 | **HC0** | 변환 트리거 + 채널 선택. `ADCH[4:0]`=채널, `AIEN[7]`=완료 IRQ enable. SW 트리거 시 **ADCH write 가 변환 시작** |
| 0x20 | **HS** | 상태 — `COCO0`(bit0, 변환 완료 플래그) |
| 0x24 | **R0** | 변환 결과 (12-bit) |
| 0x44 | **CFG** | ADLPC(저전력)/ADIV(분주)/ADLSMP(샘플시간)/MODE(해상도)/ADICLK(클럭소스). reset 0x200 |
| 0x48 | **GC** | CAL(보정 시작)/ADCO(연속변환)/AVGE(평균)/DMAEN |
| 0x4C | GS | CALF(보정 실패)/ADACT(변환 중) |
| 0x58 | CAL | 보정값 |

## 채널 ↔ 핀 (self-test 채널 선택)
- ADC1_IN0 = `GPIO_AD_B0_12` ← **SPI SDO 와 충돌! 피한다**
- ADC1_IN1 = `GPIO_AD_B0_14`, IN3 = `GPIO_AD_B1_01`, IN4 = `GPIO_AD_B1_03` … (free)
- **self-test**: 외부 HW 없음 → free 채널(예: IN1) 변환. COCO 뜨고 R0 가 0~4095(0xFFF) 범위면 동작. (floating 이라 값 자체는 노이즈 — SPI 의 TX-only 검증과 같은 성격)
- ADC 입력 핀의 IOMUXC: 아날로그 입력이라 SPI 처럼 ALT mux 거의 불필요(디지털로 드라이브만 안 하면 됨). P-6b 에서 확정.

## init 시퀀스 (RM 61.6.1, 그대로 따름)
```
1) 보정      GC[CAL]=1 → 완료까지 대기 → GS[CALF]==0 확인 (실패 아님)
2) CFG       ADICLK=bus clock, ADIV=분주, MODE=해상도(12-bit), ADLSMP=long sample
3) GC        ADCO=0 (단발 변환)
4) 변환      HC0 = 채널(ADCH) write  → 변환 시작
             HS[COCO0]==1 폴링        → 완료 대기
             R0 읽기                  → 결과 (읽으면 COCO 자동 클리어)
```

## 계획
- P-6a: 지도 ✅ (지금)
- P-6b: 클럭 게이트 + 핀 + CFG + 보정 + 변환 1회 → R0 UART 출력
- P-6c(=P-7): AUTOSAR Adc Driver API (Adc_Init / Adc_StartGroupConversion / Adc_ReadGroup) 파사드 — Spi 와 대칭

## AUTOSAR 연결
- AUTOSAR **Adc** 드라이버 = 이 ADC. P-7 에서 Spi 처럼 표준 API 로 감쌈.
- 검증 패턴도 SPI 와 동일: register bring-up(P-6b) → 표준 API(P-7).
