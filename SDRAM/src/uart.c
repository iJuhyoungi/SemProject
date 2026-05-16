/*
 * uart.c
 * -----------------------------------------------------------------------------
 * LPUART1 을 이용한 시리얼 통신 (디버그 로그 출력 + 키 입력 수신).
 *
 * 설정: 115200 bps, 8N1, 입력 클럭 PLL3 80MHz 기준.
 *   Baudrate = UART_clk / (OSR * SBR)
 *            = 80MHz / (16 * 43) ≒ 116279 bps  (≒ 115200, 오차 허용 범위)
 * -----------------------------------------------------------------------------
 */
#include "uart.h"
#include "rt1020_regs.h"
#include "led.h" // LED 제어를 위해 추가

/* LPUART1 의 클럭/핀/통신 파라미터/인터럽트를 초기화한다. */
void UART1_Init(void) {
    /* 1. Clock gating: LPUART1 모듈 클럭 ON.
     *    CCGR5 의 CG12(비트 25:24)를 11(=3)로 설정. */
    CCM_CCGR5 |= (3 << 24);

    /* 2. UART 클럭 소스/분주 선택.
     *    CSCDR1[6] UART_CLK_SEL  = 0 -> 소스 = PLL3 80MHz
     *    CSCDR1[5:0] UART_CLK_PODF = 0 -> 1분주 (그대로 80MHz 사용)
     *    두 필드를 한 번에 clear. */
    CCM_CSCDR1 &= ~((1 << 6) | 0x3F);               // 1. 클럭 소스 선택: PLL3

    /* 3. 핀 MUX: TX/RX 핀을 LPUART1 기능(ALT2)으로 연결. */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06 = 0x02;     //ALT2 — Select mux mode: ALT2 mux port: LPUART1_TX of instance: lpuart1
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07 = 0x02;     //ALT2 — Select mux mode: ALT2 mux port: LPUART1_RX of instance: lpuart1

    /* 4. 설정 변경 전 컨트롤러를 안전하게 비활성화. */
    LPUART1_CTRL = 0;

    /* 5. Baudrate 설정.
     *    BAUD[28:24] OSR = 0x0F -> 오버샘플링 16배 (레지스터 값은 실제-1)
     *    BAUD[12:0]  SBR = 43   -> 분주값.  결과: 80MHz/(16*43) ≒ 115200bps */
    LPUART1_BAUD = 0x0F000000 | 43;

    /* 6. 송수신 및 수신 인터럽트 활성화.
     *    CTRL[18] RE  = 수신부 ON
     *    CTRL[19] TE  = 송신부 ON
     *    CTRL[21] RIE = 수신 데이터 도착 시 인터럽트 발생 허용 */
    LPUART1_CTRL |= (1 << 18) |(1 << 19) | (1 << 21);       // Receiver Enable + Transmitter Enable + RIE (Receive Interrupt Enable)

    /* 7. NVIC 에서 LPUART1 인터럽트(IRQ 20)를 허용.
     *    ISER0 의 비트20 = 1. 이게 없으면 RIE 를 켜도 ISR 이 호출되지 않는다. */
    NVIC_ISER0 |= (1 << 20);                                // IRQ 20 (LPUART1) 허용
}

/* 문자 1개 송신 — 송신 버퍼가 빌 때까지 기다린 뒤 데이터를 밀어넣는다. */
void UART1_SendChar(char c) {
    /* STAT[23] TDRE(Transmit Data Register Empty)가 1이 될 때까지 폴링.
     * 이전 문자가 다 나갈 때까지 기다리는 과정 (blocking). */
    while (!(LPUART1_STAT & (1 << 23)));
    LPUART1_DATA = c;                       /* DATA 레지스터에 쓰면 하드웨어가 전송 시작 */
}

/* 널 종료 문자열을 한 글자씩 송신. */
void UART1_SendString(const char *str) {
    while (*str) {
        UART1_SendChar(*str++);
    }
}

/* 🚀 LPUART1 인터럽트 서비스 루틴 (데이터가 수신될 때마다 하드웨어가 자동 실행) */
void LPUART1_IRQHandler(void) {
    /* 수신 버퍼에 데이터가 있는지(STAT[21] RDRF, Receive Data Register Full) 확인 */
    if (LPUART1_STAT & (1 << 21)) {
        /* 데이터를 읽는 순간 하드웨어가 자동으로 RDRF 플래그를 지워줍니다 */
        char rx_data = LPUART1_DATA;

        /* 터미널에 에코(메아리) 전송 — 사용자가 친 키가 화면에 보이도록 */
        UART1_SendChar(rx_data);

        /* 재미있는 테스트: 키보드 입력으로 LED 제어 */
        if (rx_data == '1') {
            LED_On();
            UART1_SendString(" -> LED ON!\r\n");
        } else if (rx_data == '0') {
            LED_Off();
            UART1_SendString(" -> LED OFF!\r\n");
        }
    }
}

/* 🚀 32비트 정수를 16진수 문자열("0x........")로 변환하여 출력하는 함수 */
void UART1_SendHex32(uint32_t val) {
    char buffer[11];        /* "0x" + 8자리 + 널 = 11바이트 */
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[10] = '\0'; // 널 문자(문자열의 끝)

    /* 32비트를 4비트(Nibble)씩 8번 쪼개서 ASCII 문자로 변환 */
    for (int j = 0; j < 8; j++) {
        // 맨 윗자리부터 4비트씩 추출 (j=0 이 최상위 니블)
        uint32_t nibble = (val >> ((7 - j) * 4)) & 0x0F;

        if (nibble < 10) {
            buffer[2 + j] = '0' + nibble;        // 0~9 처리
        } else {
            buffer[2 + j] = 'A' + (nibble - 10); // A~F 처리
        }
    }
    UART1_SendString(buffer);
}
