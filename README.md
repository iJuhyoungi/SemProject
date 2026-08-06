# SemProject — i.MX RT1020 베어메탈 시스템 소프트웨어

> NXP i.MX RT1020 (Cortex-M7) 보드 한 대로 **부팅부터 보안까지** 임베디드 시스템
> 소프트웨어의 주요 영역을 직접 구현한 개인 프로젝트 모음입니다. 벤더 SDK 의 추상화를
> 걷어내고 레퍼런스 매뉴얼을 보며 레지스터부터 올렸고, 모든 결과는 **실제 보드에서
> 동작을 확인**한 뒤 문서로 남겼습니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![toolchain](https://img.shields.io/badge/toolchain-arm--none--eabi_%2B_CMake-lightgrey)]()
[![probe](https://img.shields.io/badge/flash-pyOCD_%2F_DAPLink-lightgrey)]()

---

## 어디부터 보면 되는가

시간이 많지 않다면 아래 세 개만 보셔도 됩니다.

| 추천 | 챕터 | 무엇을 보여주는가 |
|---|---|---|
| 1 | **[Secure Boot](SDRAM_SECURE_BOOT/)** | SHA-256 과 RSA-2048 을 **외부 라이브러리 없이 직접 구현**하고, 그 위에 fault injection 내성·measured boot·키 폐기까지 얹었습니다. 위협모델을 먼저 쓰고 **공격을 보드에서 재현한 뒤 방어 전후를 대조**했습니다 |
| 2 | **[Peripheral (AUTOSAR MCAL)](RT1020_PERIPHERAL/)** | 7종 드라이버를 베어메탈로 올리고 AUTOSAR 스타일 facade(DET / Config / WdgM)로 감쌌습니다. 자동차 SW 아키텍처를 코드로 이해한 결과입니다 |
| 3 | **[ECC / RAS](RT1020_ECC/)** | SECDED 를 직접 구현하고 **flash 에 실제로 비트를 뒤집어** 정정·검출을 실증했습니다. 메모리 신뢰성의 기본기입니다 |

---

## 챕터

| 챕터 | 주제 | 핵심 내용 |
|---|---|---|
| [UART / SDRAM / CACHE / DMA / BOOT](#저장소-구조) | 베어메탈 기초 | 클럭·핀먹싱·SEMC 로 SDRAM 초기화, 캐시 정책과 벤치마크, DMA, XIP 부팅과 재배치 |
| [SDRAM_OTA_AB](SDRAM_OTA_AB/) | 무중단 업데이트 | A/B 파티션, boot control 블록, 롤백, 호스트 업로더, 2-stage 부트로더 |
| [SDRAM_SECURE_BOOT](SDRAM_SECURE_BOOT/) | **보안** | 3단 신뢰 사슬, RSA-2048 직접 구현, anti-rollback, **fault injection 하드닝**, measured boot, 키 계층·폐기 |
| [RT1020_PERIPHERAL](RT1020_PERIPHERAL/) | 자동차 SW | MCAL 7종(ADC·PWM·ICU·CAN·SPI·GPT·WDG) + AUTOSAR facade, XBAR 루프백 검증 |
| [RT1020_FLS](RT1020_FLS/) | 스토리지 | FlexSPI NOR 드라이버(IP 커맨드 직접 제어), AUTOSAR Fls 비동기 job 모델, Fee(가비지 컬렉션·뱅크 전환) |
| [RT1020_ECC](RT1020_ECC/) | 메모리 신뢰성 | SECDED(13,8) 과 Hsiao(72,64) 직접 구현, flash 비트 주입 실증, patrol scrubbing |

---

## 하이라이트

### Secure Boot — 만든 다음 직접 공격했습니다

암호 구현으로 끝내지 않고 **위협모델(T-01 ~ T-20)을 먼저 문서화**한 뒤, 소프트웨어로
닫을 수 있는 것과 하드웨어가 필요한 것을 구분했습니다.

가장 공들인 부분은 **fault injection 하드닝**입니다. 글리치를 모사하는 하네스를 만들되
"검증 성공을 강제"하지 않고 실제 글리치가 할 수 있는 것(값 1비트 손상, 명령 1개 스킵)만
흉내내도록 설계했습니다. 그래야 하드닝 후 같은 하네스가 실패하는 것이 방어의 증거가
됩니다.

```
하드닝 전                              하드닝 후
[Verify] RSA signature FAIL            [Verify] RSA signature FAIL
[Glitch] verdict branch skipped        [Glitch] verdict branch skipped
[BL2] App B OK - jumping               [BL2] App B OK - jumping
[App B] running (slot B)   ← 뚫림      [Jump] verdict is not PASS - refusing to jump
                                       [App A] running (slot A)   ← 2차 방어선이 차단
```

위협 20건 중 13건을 차단했고, **그중 4건은 보드에서 공격을 재현해 전후를 대조**했습니다.
나머지 9건은 같은 종류의 공격이 통할 구조를 제거한 것이며 전용 시연은 만들지 않았습니다.
이 구분은 문서에 그대로 적어뒀습니다.

### 남은 한계를 실패가 아니라 경계로 다뤘습니다

정책 rollback(T-05)과 Stage 1 불변성(T-08/09)은 소프트웨어로 닫을 수 없습니다.
**되돌릴 수 없음(irreversibility)을 소프트웨어가 만들어낼 수 없기 때문**입니다. 보장
장치를 또 다른 소프트웨어에 두면 무한 회귀에 빠집니다.

퓨즈는 비가역이라 보드 한 대 환경에서는 굽지 않기로 하고, 대신
[HAB 와 OTP 결합 설계](SDRAM_SECURE_BOOT/docs/HAB_AND_OTP.md)를 문서로 남겼습니다.
Stage 2 이하 구조는 하드웨어를 도입해도 그대로 재사용됩니다.

---

## 공통 원칙

- **직접 구현합니다.** SHA-256, 2048비트 bignum 산술, RSA 검증, SECDED 인코더 모두
  스펙 문서를 보고 직접 짰습니다. 라이브러리를 부르면 배울 것이 남지 않습니다.
- **보드에서 확인합니다.** UART 로그와 실제 동작으로 검증하지 않은 것은 완료로 치지
  않았습니다. 실패 시나리오도 일부러 만들어 재현했습니다.
- **왜 그렇게 했는지 남깁니다.** 각 챕터의 `docs/` 에 설계 판단과 트러블슈팅 과정을
  기록했습니다. 잘 된 것보다 **막혔던 지점과 그것을 좁혀간 방법**에 무게를 뒀습니다.
- **한계를 명시합니다.** 못 한 것을 안 한 척하지 않고, 왜 범위 밖인지 근거를 적었습니다.

---

## 저장소 구조

```
SemProject/
├── UART/  SDRAM/  SDRAM_CACHE/  SDRAM_DMA/  SDRAM_BOOT/
│       └── 초기 베어메탈 학습 단계의 스냅샷입니다. 하나의 프로젝트가
│          UART → SDRAM → 캐시 → DMA → 부팅 순으로 커진 기록이라
│          README 가 동일하며, 폴더마다 그 시점의 전체 트리가 들어 있습니다
│
├── SDRAM_OTA_AB/          A/B 무중단 업데이트
├── SDRAM_SECURE_BOOT/     보안 (하이라이트)
├── RT1020_PERIPHERAL/     AUTOSAR MCAL
├── RT1020_FLS/            NOR flash 드라이버 + Fee
├── RT1020_ECC/            ECC / RAS
└── tools/
```

각 챕터는 독립적으로 빌드·플래시됩니다.

```bash
cd <챕터>
./build.sh          # arm-none-eabi-gcc + CMake
./flash_mcu.sh      # pyOCD (DAPLink)
```

---

## 환경

| 항목 | 내용 |
|---|---|
| 보드 | NXP MIMXRT1020-EVK (Cortex-M7 @ 500MHz) |
| 외부 메모리 | SDRAM (SEMC), FlexSPI NOR flash |
| 툴체인 | `arm-none-eabi-gcc`, CMake |
| 플래시·디버그 | pyOCD + 온보드 DAPLink |
| 호스트 테스트 | Unity (x86 gcc 로 별도 빌드) |
| 개발 환경 | Linux |

---

## 진행 상황

여섯 챕터 모두 보드 검증을 마치고 봉인했습니다. 다음은 보안 쪽 확장입니다.

- [ ] Secure boot 파서 fuzzing (libFuzzer) — 이미지 헤더, PKCS#1 v1.5 패딩, 메타데이터·인증서 파서
- [ ] Remote attestation — measured boot 을 원격 검증으로 확장
- [ ] HAB / OTP 실적용 — 개발용 보드 별도 확보 후

---

_학습 및 포트폴리오 목적입니다. 제품에 그대로 쓰기 위한 코드가 아닙니다._
