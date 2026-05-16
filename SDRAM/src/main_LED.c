/*
 * main_LED.c
 * -----------------------------------------------------------------------------
 * [초기 학습용 단독 프로토타입 — 현재 빌드에는 미포함]
 *
 * "레지스터 직접 두드려 LED 깜빡이기"를 단일 파일로 처음 연습한 버전.
 * 이후 led.c / main.c 로 모듈화되며 역할이 분리되었다.
 * main() 이 중복 정의되므로 main.c 와 동시에 빌드할 수 없다 — 학습 기록용으로 보존.
 * -----------------------------------------------------------------------------
 */
#include <stdint.h>

/* 레지스터 절대 주소 매크로 */
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)          //칩 내부의 많은 모듈 중에서 특정 모듈에만 선택적으로 전원을 공급
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0)          //선택한 핀을 어떤 역할로 쓸지 결정
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)          //핀을 GPIO로 사용할 때, 입력으로 쓸지 출력으로 쓸지 결정
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)          //핀의 출력 HIGH/LOW 값을 쓰는 데이터 레지스터

/* 아주 단순한 지연 함수.
 * volatile 카운터: 컴파일러가 "아무 일도 안 하는 루프"라며 통째로 없애버리는 것을 방지. */
void delay_loop(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");           /* nop 1개 = 1사이클 소비. 정밀한 시간은 아님 */
    }
}

int main(void)
{
    /* 0. 디버거가 접속할 시간을 벌어주는 생명줄 (안전장치) */
    delay_loop(5000000);

    /* 1. CCM: GPIO1 모듈 클럭 활성화 (이게 없으면 칩이 즉사합니다!)
     *    CCGR1 의 CG13(비트 27:26)을 11(=3)로. */
    CCM_CCGR1 |= (3 << 26);

    /* 2. IOMUXC: 해당 핀을 GPIO 모드로 설정 (ALT5) */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05;

    /* 3. GPIO: 5번 핀을 출력(Output)으로 설정 (9번에서 5번으로 수정) */
    GPIO1_GDIR |= (1 << 5);             //데이터시트 상 1은 출력을 의미

    /* 무한 루프: LED 깜빡이기 */
    while (1) {
        GPIO1_DR |= (1 << 5);   // LED 켜기 (HIGH) — DR[5] = 1
        delay_loop(500000);    // 대기

        GPIO1_DR &= ~(1 << 5);  // LED 끄기 (LOW) — DR[5] = 0
        delay_loop(500000);    // 대기
    }

    return 0;
}
