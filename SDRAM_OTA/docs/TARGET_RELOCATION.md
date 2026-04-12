# NXP i.MX RT1020 베어메탈 클럭 튜닝 및 Targeted Relocation (132MHz)

## 1. 프로젝트 개요
본 프로젝트는 NXP i.MX RT1020 EVK 보드에서 OS나 HAL 라이브러리 없이 레지스터를 직접 제어하는 베어메탈(Bare-Metal) 펌웨어입니다.
외부 플래시 메모리(XiP) 실행 중 시스템 클럭을 변경할 때 발생하는 통신 단절(HardFault)을 방지하기 위해, **클럭 제어 함수만 칩 내부의 초고속 명령어 RAM(ITCM)으로 재배치(Targeted Relocation)**하여 안전하게 132MHz로 부스팅하는 기술을 구현했습니다.

---

## 2. 핵심 아키텍처 및 트러블슈팅

### 🚀 Targeted Relocation (부분 재배치) 및 최적화 방어
* **문제:** Flash(XiP) 영역에서 클럭 관련 레지스터(`CCM_CBCDR` 등)를 조작하면 Flash 컨트롤러와의 통신이 순간적으로 끊겨 시스템이 즉사(HardFault)함.
* **해결:** 치명적인 구간인 `Clock_Init_132MHz()` 함수만 링커 스크립트의 `.ramfunc` 섹션(ITCM, 0x00000000 대역)으로 복사하여 실행.
* **`noinline` 속성 부여 (신의 한 수):** 함수 크기가 작아 컴파일러가 `main` 함수 내부로 인라인(Inline) 최적화를 해버리면 Flash에서 실행되는 문제가 발생함. 이를 막기 위해 `__attribute__((section(".ramfunc"), noinline))`을 선언하여 강제로 브랜치(Branch)를 생성하고 ITCM으로 완벽하게 점프하도록 설계함.

### ⏱️ 132MHz '초안전' 클럭 스위칭 정석 시퀀스
1. **우회로(Bypass) 확보:** 클럭 소스를 바꿀 때는 먼저 대체 경로(`PERIPH_CLK_SEL = 1`)로 우회시킴.
2. **안전 분주비 세팅:** Boot ROM이 켜둔 528MHz Sys PLL을 4분주(`AHB_PODF = 3`)하여 132MHz 버스 속도 세팅.
3. **핸드쉐이크 대기:** 레지스터 설정 후 하드웨어가 "변경 완료" 신호를 보낼 때까지 `while (CCM_CDHIPR & ...)`로 철저히 대기.

---

## 3. 핵심 레지스터 맵 (Memory Map)

* **`CCM_CBCMR` (0x400FC018)**: 버스 클럭 멀티플렉서 레지스터
    * `PRE_PERIPH_CLK_SEL` 비트(18~19)를 `00`으로 설정하여 **528MHz Sys PLL** 선택.
* **`CCM_CBCDR` (0x400FC014)**: 버스 클럭 분주기 레지스터
    * `AHB_PODF` 비트(10~12)를 `3`으로 설정하여 **4분주** 적용.
    * `PERIPH_CLK_SEL` 비트(25)로 메인 경로와 우회 경로 스위칭.
* **`CCM_CDHIPR` (0x400FC048)**: 핸드쉐이크 상태 레지스터
    * 하드웨어 클럭 스위칭/분주비 변경 완료 여부를 모니터링 (1: 진행 중, 0: 완료).

---

## 4. 메인 소스 코드 (`main.c`)

```c
#include <stdint.h>

/* 레지스터 절대 주소 매크로 */
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0)
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)

/* GDB 디버깅용 마커 변수 */
volatile uint32_t g_clock_init_marker = 0;

/* 132MHz 클럭 설정 함수 (ITCM 대피 & noinline 최적화 방어) */
__attribute__((section(".ramfunc"), noinline))
void Clock_Init_132MHz(void) {
    g_clock_init_marker = 0x11111111; // 시작 마커

    /* 1. 안전하게 4분주 기어 장착 (AHB_PODF = 3) */
    CCM_CBCDR &= ~(7 << 10); 
    CCM_CBCDR |= (3 << 10);  
    while (CCM_CDHIPR & (1 << 1)); // 변경 완료 대기

    /* 2. CPU 클럭 소스를 528MHz Sys PLL로 변경 */
    CCM_CBCMR &= ~(3 << 18); 

    /* 3. 우회로를 닫고 메인 경로(PERIPH_CLK_SEL = 0)로 스위칭 */
    CCM_CBCDR &= ~(1 << 25); 
    while (CCM_CDHIPR & (1 << 5)); // 스위칭 완료 대기
    
    /* 최종 속도: 528MHz / 4 = 132MHz */
    g_clock_init_marker = 0x22222222; // 완료 마커
}

/* 플래시 메모리에서 실행되는 지연 함수 */
void delay_loop(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop"); 
    }
}

/* 메인 함수 (Flash 영역 실행) */
int main(void)
{
    /* 초기 지연 (디버거 접속용) */
    delay_loop(2000000);

    /* 🚀 Flash(0x6000xxxx)에서 ITCM(0x00000000)으로 점프하여 안전하게 스위칭 */
    Clock_Init_132MHz();        

    /* 클럭 변경 완료 후 다시 Flash로 복귀하여 GPIO 및 클럭 활성화 */
    CCM_CCGR1 |= (3 << 26); 
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05; 
    GPIO1_GDIR |= (1 << 5);             

    /* 무한 루프: 132MHz 스피드로 실행 */
    while (1) {
        GPIO1_DR |= (1 << 5);   // ON
        delay_loop(500000);    
        
        GPIO1_DR &= ~(1 << 5);  // OFF
        delay_loop(500000);    
    }
    return 0;
}
