# i.MX RT1020 OTA A/B Bootloader 작업 정리

## 1. 작업 목표

이번 작업의 목표는 아래 두 단계를 순서대로 밟는 것이었다.

### C-1. Stable dummy app 기반 OTA/A-B 프레임 검증
- Bootloader가 Slot A / Slot B를 선택할 수 있어야 한다.
- 선택된 slot이 invalid이면 반대 slot으로 fallback 해야 한다.
- Boot metadata(active_slot, pending_slot, boot_success, boot_attempts)를 이용해 이후 rollback skeleton으로 확장할 수 있어야 한다.

### C-2. SDRAM 연계 app를 실제 slot payload로 교체
- Slot A를 real SDRAM-reloc app로 교체
- Slot B는 initially dummy app로 유지하여 복구용 golden image 역할
- 이후 cache / DMA / peripheral로 확장

현재 기준으로 정리하면:
- C-1 완료
- C-2.1 완료 (Slot A SDRAM-reloc app 성공)
- C-2.2 완료 (정적 metadata 기반 trial / rollback skeleton 성공)
- C-2.3은 아직 미진행 또는 보류 (runtime metadata flash write는 아직 미완성)

---

## 2. 왜 기존 프로젝트를 그대로 쓰지 않고 단계적으로 분리했는가

기존 프로젝트에는 아래 요소들이 한꺼번에 섞여 있었다.

- SDRAM init
- SDRAM relocation
- Cache 실험
- DMA 실험
- OCRAM / ITCM / .ramfunc 실험
- HardFault 분석 코드
- UART IRQ 흔적
- Bootloader와 결합된 구조

이 상태에서는 장애가 발생했을 때 원인을 분리하기가 매우 어려웠다.

예를 들어 아래 중 어디가 원인인지 즉시 구분되지 않았다.

- Bootloader jump 문제
- Vector table / MSP / PC 문제
- SDRAM bring-up 문제
- Pre-relocation code / rodata 문제
- Cache / MPU 문제
- DMA 문제
- Metadata / rollback 문제

그래서 먼저 다음 원칙으로 구조를 분리했다.

1. C-1에서는 dummy payload만 사용
2. Bootloader / metadata / slot app를 분리
3. A/B 프레임과 fallback을 먼저 살림
4. 그다음에만 SDRAM real payload를 올림

즉 먼저 부팅 프레임을 살리고, 그 위에 SDRAM payload를 올리는 전략을 사용했다.

---

## 3. 현재 프로젝트 구조

현재 프로젝트는 크게 4개 축으로 구성된다.

- bootloader/
- app_slot_a/
- app_slot_b/
- bootctrl/

### 3.1 bootloader
역할:
- boot metadata 읽기
- slot 선택
- app 유효성 검사
- fallback 처리
- app jump

### 3.2 app_slot_a
역할:
- 현재는 real SDRAM-reloc app
- bootloader가 jump하면
  - Reset_Handler
  - SDRAM init
  - .text / .data / .ramfunc relocation
  - main()
  - heartbeat 출력

### 3.3 app_slot_b
역할:
- 현재는 dummy app
- fallback / golden image 역할
- 복구용 기준 이미지

### 3.4 bootctrl
역할:
- boot metadata 초기값 저장
- BOOT_CTRL_ADDR = 0x603F0000
- bootloader와 분리된 별도 이미지

---

## 4. 재사용한 것과 비활성화한 것

### 4.1 재사용한 것
- bootloader/src/main.c 의 jump 골격
- bootloader/include/boot_policy.h
- bootloader/src/boot_policy.c
- shared/src/boot_header.c
- shared/src/startup_MIMXRT1020.c
- shared/src/system_MIMXRT1020.c
- shared/src/uart.c
- shared/src/led.c
- shared/src/semc.c
- shared/src/mpu.c
- shared/src/fault.c

### 4.2 C-1 / C-2 초반에서 의도적으로 제한한 것
- DMA 사용
- cache re-enable 실험
- peripheral 연동
- runtime metadata flash write
- Slot B real SDRAM payload 전환

즉, 현재까지는 payload 안정화와 A/B 프레임 안정화가 중심이었다.

---

## 5. Boot policy 및 metadata 구조

### 5.1 주요 정의

아래와 같은 핵심 정의를 사용했다.

    BOOT_CTRL_MAGIC   = 0x424F4F54u   ('BOOT')
    SLOT_A            = 0u
    SLOT_B            = 1u
    SLOT_NONE         = 0xFFFFFFFFu
    APP_SLOT_A_ADDR   = 0x60040000u
    APP_SLOT_B_ADDR   = 0x60200000u
    BOOT_CTRL_ADDR    = 0x603F0000u
    BOOT_SUCCESS_NO   = 0u
    BOOT_SUCCESS_YES  = 1u
    BOOT_MAX_ATTEMPTS = 1u

### 5.2 boot metadata 구조체

아래 구조체를 사용했다.

    typedef struct {
        uint32_t magic;
        uint32_t active_slot;
        uint32_t pending_slot;
        uint32_t boot_success;
        uint32_t boot_attempts;
    } boot_ctrl_t;

### 5.3 필드 의미

- active_slot
  - 현재 정상 부팅 기준 slot
- pending_slot
  - 다음 부팅에서 trial 대상이 되는 slot
  - 없으면 SLOT_NONE
- boot_success
  - 직전 trial 성공 여부
- boot_attempts
  - trial 시도 횟수

### 5.4 Bootloader 선택 로직

현재 Boot_SelectSlot()의 흐름은 다음과 같다.

1. magic이 맞지 않으면 Slot A 기본 선택
2. pending_slot이 유효하고, boot_success == BOOT_SUCCESS_NO이며, boot_attempts < BOOT_MAX_ATTEMPTS이면 pending slot 선택
3. trial 실패 또는 시도 횟수 초과 시 active slot로 rollback
4. 최종적으로 선택된 slot의 base address를 반환

즉 현재 구조는 아래 skeleton을 포함한다.

- 기본 선택
- pending trial 선택
- rollback skeleton
- fallback 전제 구조

### 5.5 Bootloader main 동작

현재 bootloader main()의 흐름은 다음과 같다.

1. UART1_Init()
2. LED_Init()
3. bootloader banner 출력
4. metadata 읽기
5. slot 선택
6. 선택 slot / base 로그 출력
7. 선택 slot invalid면 반대 slot fallback
8. app로 점프

점프 직전에는 아래 정리를 수행한다.

- cpsid i
- SysTick 정지
- NVIC_ICER0 clear
- NVIC_ICPR0 clear
- SCB_VTOR = app_address
- app vector[0]의 MSP 적용
- BX로 app reset handler 진입

즉 bootloader → app jump 경로는 현재 안정화된 상태다.

### 5.6 Dummy app 구조

#### Slot A dummy app
초기 C-1 단계에서는 Slot A도 dummy였다.

출력:
- [APP-A] Dummy app started
- [APP-A] boot confirmed hook point
- [APP-A] heartbeat

#### Slot B dummy app
현재 Slot B는 여전히 dummy app이다.

출력:
- [APP-B] Dummy app started
- [APP-B] boot confirmed hook point
- [APP-B] heartbeat

현재 Slot B는 golden fallback image 역할을 한다.

### 5.7 Startup 분기 정리

shared/src/startup_MIMXRT1020.c 는 현재 3분기 구조다.

- IS_BOOTLOADER
- IS_DUMMY_APP
- 그 외 = real SDRAM-reloc app

#### IS_BOOTLOADER
- .data 복사
- .bss 초기화
- bootloader main() 진입

#### IS_DUMMY_APP
- .data 복사
- .bss 초기화
- dummy app main() 진입
- SDRAM bring-up 없음
- relocation 없음

#### real SDRAM-reloc app
- SystemInit()
- Clock_Init_132MHz()
- UART1_Init()
- 초기 로그 출력
- SDRAM_Clock_Init()
- SDRAM_Pin_Init()
- SDRAM_Init_Sequence()
- .text relocation
- .data relocation
- .ramfunc relocation
- .bss 초기화
- main() 진입

이 구조를 통해:
- bootloader
- dummy payload
- real SDRAM payload

를 하나의 공용 startup으로 관리하게 되었다.

### 5.8 UART 사용 방식

현재 프로젝트 진행상 UART는 사실상 polling 방식으로 사용하고 있다.

사용 API:
- UART1_Init()
- UART1_SendChar()
- UART1_SendString()
- UART1_SendHex32()

진행 과정에서 UART IRQ 흔적이 남아 있어 혼선을 준 적은 있었지만, 현재까지의 핵심 성공 경로는 초기 로그 / bootloader 로그 / app 로그 출력에 있다.

### 5.9 boot_ctrl 분리

초기에는 .boot_ctrl 를 bootloader 이미지 내부에 넣었고, 이 때문에 다음 문제가 있었다.

- bootloader.bin 이 4MB까지 커짐
- 플래싱 속도가 지나치게 느려짐
- bootloader core와 metadata가 강결합됨

이후 .boot_ctrl 를 bootloader에서 분리하여 별도 bootctrl/ 타깃으로 분리했다.

#### 현재 구조
- bootloader.elf → bootloader core
- boot_ctrl.bin → metadata 초기값
- BOOT_CTRL_ADDR = 0x603F0000

즉 bootloader와 metadata를 분리하여:
- bootloader는 작게 유지
- boot metadata는 별도 관리

구조로 바꾸었다.

---

## 6. 중간에 겪었던 주요 문제와 해결

### 6.1 Dummy app 빌드 에러

#### 증상
app_slot_a / app_slot_b 링크 시 아래 심볼이 없어서 실패했다.

- SDRAM_Clock_Init
- SDRAM_Pin_Init
- SDRAM_Init_Sequence
- __text_start__
- __text_end__
- __text_load_start__
- __ramfunc_start__
- __ramfunc_end__
- __ramfunc_load_start__

#### 원인
dummy app인데도 shared startup이 real SDRAM-reloc app 경로를 타고 있었기 때문이다.

#### 해결
IS_DUMMY_APP 분기를 추가하여:
- dummy app는 .data / .bss만 초기화
- SDRAM init 안 함
- relocation 안 함

구조로 수정했다.

---

### 6.2 bootloader.bin 플래시가 너무 느림

#### 증상
bootloader 플래시에 15분 이상 소요되었다.

#### 원인
초기 구조에서 .boot_ctrl 가 0x603F0000 에 있었고,
objcopy -O binary 가 중간 hole까지 포함해 큰 .bin 을 생성했기 때문이다.

실제 예:
- bootloader.bin = 4.0MB
- bootloader.elf = 35KB

#### 해결
- bootloader는 bootloader.elf 로 직접 flash
- .boot_ctrl 는 bootctrl/ 로 분리
- boot_ctrl.bin 을 별도로 flash

즉 bootloader core와 metadata를 분리했다.

---

### 6.3 boot_ctrl.ld syntax error

#### 증상
boot_ctrl.ld:1: syntax error

#### 원인
데이터 blob 전용 linker script에 ENTRY(0)을 넣었기 때문

#### 해결
- ENTRY(0) 제거
- .boot_ctrl 섹션만 유지

---

### 6.4 Slot A real app 전환 시 SDRAM_* unresolved

#### 증상
app_slot_a 를 real app로 바꾸자 링크 시 SDRAM_Clock_Init, SDRAM_Pin_Init, SDRAM_Init_Sequence unresolved 발생

#### 원인
real SDRAM app startup 경로에서 semc.c 구현이 필요했기 때문

#### 해결
- shared/src/semc.c 구현을 복구 / 링크

---

### 6.5 app jump 후 첫 로그도 없이 재부팅

#### 증상
- bootloader는 [BOOT] Jumping to app... 까지 출력
- app 첫 로그가 하나도 안 보임
- 다시 bootloader banner가 뜸

#### 초기 오해
- clock 문제
- hardfault logger 문제
- UART 문제

#### 실제 핵심 원인
pre-relocation 단계에서 UART1_SendString()에 넘기는 문자열 리터럴이 .rodata 에 있었고, 현재 .rodata 가 SDRAM 쪽에 배치되어 있었기 때문에 아직 relocate 전의 문자열을 접근하다가 터진 것이다.

즉:
- 함수는 FLASH에서 실행
- 문자열 데이터는 SDRAM에 있다고 가정
- 하지만 아직 relocation 전
- 따라서 pre-relocation 로그 출력 시 crash / reset

#### 해결
app_slot_a.ld 에서 .rodata 를 .boot_text 와 함께 FLASH에 배치하도록 변경했다.

##### 변경 전
- .boot_text → FLASH
- .text + .rodata → SDRAM

##### 변경 후
- .boot_text + .rodata → FLASH
- .text만 → SDRAM

이 수정 후:
- Reset_Handler 로그 출력 성공
- SDRAM is Ready 출력 성공
- main entered 출력 성공
- heartbeat 정상

---

### 6.6 Clock_Init_132MHz 오해

pre-relocation .rodata 문제를 해결한 뒤에는 Clock_Init_132MHz()를 다시 주석 해제해도 정상 동작했다.

즉 당시의 진짜 원인은 clock이 아니라,
pre-relocation rodata 배치 문제였다.

---

### 6.7 runtime metadata write(C-2.3) 시도 중 혼선

#### 시도 내용
- bootctrl_flash.h / .c
- BootCtrl_MarkTrialAttempt()
- BootCtrl_ConfirmSlot()
- Flash_EraseBootCtrlSector()
- Flash_ProgramBootCtrl()

구조를 추가하려 했다.

#### 문제
현재 프로젝트는 아래처럼 shared 전체를 bootloader / app들이 같이 물고 있다.

    file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

즉 shared/src 에 새 파일을 넣으면:
- bootloader
- app_slot_a
- app_slot_b

모두 함께 영향을 받는다.

이 때문에 runtime metadata write를 공용으로 넣으려다가:
- 빌드 / 링크 이슈
- stub 정의 문제
- 복구 혼선

이 발생했다.

#### 결론
C-2.3 runtime metadata flash write는 아직 공용 shared에 넣을 단계가 아니다.
나중에 전용 파일 / 전용 링크 범위 / 전용 flash write 루틴으로 좁혀서 다시 시도해야 한다.

---

### 6.8 black screen인데 실제 원인은 serial terminal 상태였던 경우

한 번은 코드가 아니라 serial terminal이 잠시 안 뜬 것이 원인이었다.
터미널을 껐다 켜니 다시 로그가 정상적으로 보였다.

즉 black screen이 항상 코드 문제는 아니며, 아래도 같이 점검해야 한다.

- GTKTerm 재시작
- 포트 점유 상태 확인
- 보드 reset 타이밍
- 터미널 open timing

---

## 7. 수행한 테스트와 결과

### 테스트 1: 기본 Slot A 부팅

#### metadata
- active_slot = SLOT_A
- pending_slot = SLOT_NONE
- boot_success = BOOT_SUCCESS_YES
- boot_attempts = 0

#### 기대 결과
- Slot A 선택
- Slot A app 실행

#### 실제 결과
성공

---

### 테스트 2: Slot B 강제 선택

#### 방법
- 정책 또는 metadata로 Slot B 선택

#### 기대 결과
- Slot B 선택
- Slot B app 실행

#### 실제 결과
성공

---

### 테스트 3: fallback

#### 조건
- 선택된 slot invalid
- 반대 slot valid

#### 기대 결과
- invalid slot 감지
- 반대 slot fallback
- 반대 slot app 실행

#### 실제 결과
성공

---

### 테스트 4: 정적 metadata 기반 trial boot

#### metadata
- active_slot = SLOT_B
- pending_slot = SLOT_A
- boot_success = BOOT_SUCCESS_NO
- boot_attempts = 0

#### 기대 결과
- Slot A를 trial 대상으로 선택

#### 실제 결과
성공

---

### 테스트 5: 정적 metadata 기반 rollback skeleton

#### metadata
- active_slot = SLOT_B
- pending_slot = SLOT_A
- boot_success = BOOT_SUCCESS_NO
- boot_attempts = 1

#### 기대 결과
- Slot B로 rollback 선택

#### 실제 결과
성공

즉 C-2.2까지는 정적 metadata 기반 trial / rollback skeleton 검증이 완료되었다.

---

## 8. 현재 상태 평가

현재 상태는 다음과 같다.

### 완료
- C-1 stable dummy app 기반 OTA/A-B 프레임 구축
- Slot A / Slot B 선택 검증
- invalid slot fallback 검증
- bootctrl 분리
- 정적 metadata 기반 trial / rollback skeleton 검증
- Slot A real SDRAM-reloc app 전환 성공
- pre-relocation .rodata 문제 해결
- Clock_Init_132MHz() 포함 상태에서도 정상 동작 확인

### 현재 유지 중
- Slot A = real SDRAM-reloc app
- Slot B = dummy golden image
- bootloader = 정상 동작
- bootctrl = 정적 metadata 초기값 공급 전용

### 아직 미완성
- runtime metadata flash write
- real OTA-like confirm / rollback persistence
- Slot B real SDRAM payload 전환
- cache / DMA / peripheral 재도입

즉 현재는:
- C-1 완료
- C-2.1 완료
- C-2.2 완료
- C-2.3 보류

상태다.

---

## 9. 다음 단계(C-2.3 이후) 방향

가장 안전한 다음 단계는 아래와 같다.

### 9.1 baseline 고정
현재 정상 상태를 먼저 고정한다.

권장:
- 소스 백업
- 커밋
- 정상 로그 저장
- 빌드 산출물 hash / size 기록

### 9.2 runtime metadata flash write 재시도
단, 다음 원칙을 지켜야 한다.

- shared/*.c 공용으로 막 넣지 말 것
- 영향 범위를 좁힐 것
- 전용 파일 / 전용 링크 범위로 넣을 것
- XIP 상태 flash write 제약 고려
- 필요 시 .ramfunc / ITCM 사용

### 9.3 Slot B real payload 전환
runtime metadata update가 안정화되면,
그 다음에 Slot B를 real SDRAM app로 전환한다.

### 9.4 cache / DMA / peripheral 재도입
최종적으로:
1. runtime metadata write
2. Slot B real payload
3. cache
4. DMA
5. peripheral

순으로 가는 것이 안전하다.

---

## 10. 현재 가장 중요한 기술적 교훈

### 10.1 먼저 부팅 프레임을 살리고, 그 위에 payload를 올려야 한다
A/B 구조에서는 bootloader / metadata / fallback skeleton이 먼저 살아 있어야 한다.

### 10.2 함수만 FLASH에 두는 것으로는 충분하지 않다
pre-relocation 단계에서 쓰는 문자열 리터럴도 함께 FLASH에 두어야 한다.

즉:
- .boot_text만 FLASH
- .rodata는 SDRAM

구조는 초기 로그 단계에서 위험할 수 있다.

### 10.3 Slot 하나는 반드시 golden image로 남겨두는 게 좋다
Slot A를 real payload로 전환할 때 Slot B를 dummy golden image로 유지했기 때문에 복구 / rollback 검증이 쉬웠다.

### 10.4 runtime metadata update는 나중 문제다
정적 metadata 조작만으로도 C-2.2까지 충분히 skeleton 검증이 가능했다.
진짜 flash write는 영향 범위가 크므로 baseline 고정 후 별도로 진행해야 한다.

### 10.5 black screen은 코드 문제만이 아니다
serial terminal 상태 문제도 반드시 함께 점검해야 한다.

---

## 11. 현재 안정 baseline 정의

현재 안정 baseline은 다음과 같다.

### bootloader
- boot banner 출력 정상
- metadata 읽기 정상
- slot 선택 정상
- fallback 정상
- app jump 정상

### app_slot_a
- real SDRAM-reloc app
- Reset_Handler 로그 정상
- SDRAM is Ready 로그 정상
- main entered 로그 정상
- heartbeat 정상

### app_slot_b
- dummy app
- fallback / golden image 역할
- heartbeat 정상

### bootctrl
- 정적 metadata 초기값 제공
- 별도 boot_ctrl.bin

즉 현재 baseline은:

    Bootloader + Slot A real SDRAM-reloc payload + Slot B dummy fallback + bootctrl 분리 구조가 정상 동작하는 안정 상태

이다.

---

## 12. 앞으로 작업 시 주의사항

1. 한 번에 여러 파일을 바꾸지 말 것
2. baseline 커밋 후 실험할 것
3. shared 공용 소스 추가는 특히 조심할 것
4. pre-relocation code는 rodata까지 함께 고려할 것
5. black screen이면 코드와 terminal 둘 다 점검할 것

---

## 13. 요약

이번 작업을 통해 달성한 것은 다음과 같다.

1. Bootloader / Slot A / Slot B / bootctrl 분리
2. dummy 기반 OTA A/B 프레임 완성
3. Slot A / Slot B 선택 및 fallback 검증
4. 정적 metadata 기반 trial / rollback skeleton 검증
5. Slot A를 real SDRAM-reloc app로 성공적으로 전환
6. pre-relocation .rodata 배치 문제가 핵심 장애였음을 확인하고 해결
7. Slot B를 dummy golden image로 유지해 복구 가능한 구조 확보

현재 상태는 한 줄로 요약하면 다음과 같다.

    C-1과 C-2.1, C-2.2까지는 성공적으로 완료되었고, 현재는 안정 baseline을 확보한 상태이며, 이후 과제는 runtime metadata flash write(C-2.3)와 Slot B real payload 전환이다.

---

## 14. 현재 기준 추천 다음 작업

가장 추천하는 다음 작업은 아래 순서다.

1. 현재 baseline을 커밋
2. 로그와 이미지 hash 저장
3. C-2.3을 전용 범위에서만 재시도
4. Slot B real payload 전환
5. cache / DMA / peripheral 재도입

---

## 15. baseline 백업 권장 명령 예시

아래 명령으로 baseline을 저장할 수 있다.

    git status
    git add .
    git commit -m "Recovered stable baseline: bootloader + slot A SDRAM + slot B dummy"

    sha256sum build/bootloader/bootloader.elf
    sha256sum build/app_slot_a/app_slot_a.elf
    sha256sum build/app_slot_b/app_slot_b.elf
    sha256sum build/bootctrl/boot_ctrl.bin

---

## 16. 현재 Slot 역할 정리

- Slot A
  - real SDRAM-reloc payload
  - 현재 실험 대상 / 실제 app 역할

- Slot B
  - dummy app
  - fallback / rollback / golden image 역할

- bootctrl
  - 정적 metadata 공급 전용

- bootloader
  - 정책 판단 / sanity check / fallback / jump

---

## 17. 현재 완료 판정

다음 항목이 모두 성공했으므로 현재까지의 작업은 유의미하게 완료된 상태다.

- bootloader 배너 출력 성공
- Slot A 선택 성공
- Slot B 선택 성공
- fallback 성공
- trial skeleton 성공
- rollback skeleton 성공
- Slot A SDRAM init 성공
- relocation 성공
- main 진입 성공
- heartbeat 성공
- Clock_Init_132MHz() 포함 상태에서도 정상 동작

---

## 18. 남은 위험 포인트

아직 해결하지 않은 위험 포인트는 다음과 같다.

- runtime flash write 시 XIP 충돌
- .ramfunc 활용 범위
- metadata sector erase / program
- Slot B real payload 전환 시 복구 기준 상실 가능성
- cache / DMA 재도입 시 MPU / 메모리 속성 문제 재발 가능성

---

## 19. 확실히 기억할 핵심 원인

이번 전체 과정에서 가장 중요한 원인 분석 포인트는 다음이었다.

    Slot A real app 전환 후 발생한 early reset의 핵심 원인은 pre-relocation 단계에서 참조하는 문자열 리터럴(.rodata)이 SDRAM 쪽에 배치되어 있었던 것이며, 이를 .boot_text 와 함께 FLASH에 두도록 수정하면서 해결되었다.

이건 이후 어떤 XIP / relocation / bootloader 구조를 하더라도 계속 중요하게 남는 포인트다.

---

## 20. 결론

현재 프로젝트는 다음 상태다.

- 부팅 프레임 정상
- fallback 정상
- Slot A real payload 정상
- Slot B fallback 이미지 정상
- baseline 확보 완료

즉 지금은 무리해서 기능을 더 넣기보다, 이 baseline을 기준점으로 삼아 다음 기능을 작은 단위로 붙이는 것이 맞다.

---

## 21. 최종 한 줄 요약

    현재 상태는 "Bootloader + Slot A SDRAM-reloc real app + Slot B dummy fallback + bootctrl 분리"가 모두 정상 동작하는 안정 baseline이며, 가장 중요한 기술적 돌파구는 pre-relocation .rodata 를 FLASH에 함께 배치하도록 바꿔 early reset 문제를 해결한 것이다.

