# i.MX RT1020 Bare-Metal Project Skeleton

이 디렉토리는 i.MX RT1020(Cortex-M7)용 bare-metal 프로젝트 골격입니다.

이 골격은 벤더 HAL이나 MCUXpresso SDK 없이 레지스터 수준에서 기능을 추가하는 것을 전제로 합니다.

## 포함 내용

- `src/`: 애플리케이션 코드와 시스템 초기화
- `include/`: 최소 디바이스/시스템 헤더
- `startup/`: 벡터 테이블과 `Reset_Handler`
- `linker/`: FlexSPI XIP 기준 GNU ld 스크립트
- `docs/`: 프로젝트 가정과 확장 포인트 문서

## 설계 원칙

- `fsl_*` 계열 벤더 드라이버를 사용하지 않음
- 스타트업과 메모리 배치를 직접 관리
- 필요한 주변장치는 개별 bare-metal 드라이버로 추가

## 기본 가정

- 툴체인: `arm-none-eabi-gcc`
- 부트 방식: 외부 FlexSPI NOR XIP
- 메모리 분할:
  - `ITCM`: 64 KB at `0x00000000`
  - `DTCM`: 64 KB at `0x20000000`
  - `OCRAM`: 128 KB at `0x20200000`
- 앱 코드 배치:
  - Flash config block: `0x60000000`
  - IVT/Boot Data: `0x60001000`
  - `.text`: `0x60002000` 이후

## 빌드

```sh
make
```

또는

```sh
cmake -S . -B build-cmake
cmake --build build-cmake
```

## 주의 사항

- `src/boot_header.c`의 FlexSPI NOR 설정 블록은 템플릿입니다.
- 실제 보드에 탑재된 NOR Flash 파트에 맞는 LUT/타이밍/사이즈로 반드시 교체해야 ROM 부트가 정상 동작합니다.
- 현재 `SystemInit()`은 최소 초기화만 수행합니다. 클럭 트리, 핀 mux, MPU, 캐시 정책, 주변장치 초기화는 프로젝트 요구사항에 맞게 추가해야 합니다.
