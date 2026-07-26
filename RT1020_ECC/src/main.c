#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "Ecc.h"

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

/* E-2: (13,8) SECDED 를 RAM 안에서 전수 검증한다. 모든 데이터(0~255)에 대해
 * ① 무오류 → NO_ERROR + 값 일치, ② 단일비트 13개 전수 → CORRECTED + 값 복구,
 * ③ 이중비트(인접 쌍) → UNCORRECTABLE 검출. 하드웨어(flash) 없이 수학만 격리 검증. */
static void report_ecc_selftest(void)
{
    uint32_t okClean=0u;
    uint32_t okCorr=0u;
    uint32_t okDet=0u;
    uint32_t fail=0u;
    uint16_t d;

    UART1_SendString("[ECC] === E-2 (13,8) SECDED self-test ===\r\n");

    for(d=0u;d<256u;++d){
        uint8_t data=(uint8_t)d;
        uint16_t cw=Ecc_Encode(data);
        uint8_t out;
        uint8_t b;

        if((Ecc_Decode(cw,&out)==ECC_NO_ERROR)&&(out==data)){
            okClean++;
        }
        else{
            fail++;
        }

        for(b=0u;b<13u;++b){
            uint16_t bad=(uint16_t)(cw^(1u<<b));
            if((Ecc_Decode(bad,&out)==ECC_CORRECTED)&&(out==data)){
                okCorr++;
            }else{
                fail++;
            }
        }

        for(b=0u;b<12u;++b){
            uint16_t bad=(uint16_t)(cw^(1u<<b)^(1u<<(b+1u)));
            uint8_t o2;
            if(Ecc_Decode(bad,&o2)==ECC_UNCORRECTABLE){
                okDet++;
            }else{
                fail++;
            }
        }
    }

    UART1_SendString("[ECC]   clean   OK = ");  UART1_SendHex32(okClean);  /* 기대 0x100  = 256  */
    UART1_SendString("\r\n[ECC]   correct OK = "); UART1_SendHex32(okCorr);  /* 기대 0xD00  = 3328 */
    UART1_SendString("\r\n[ECC]   detect  OK = "); UART1_SendHex32(okDet);   /* 기대 0xC00  = 3072 */
    UART1_SendString("\r\n[ECC]   fail       = "); UART1_SendHex32(fail);    /* 기대 0        */
    UART1_SendString(fail == 0u ? "\r\n[ECC]   ALL PASS\r\n" : "\r\n[ECC]   FAIL\r\n");
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[ECC] RT1020 ECC / RAS — hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    report_ecc_selftest();

    uint32_t beat = 0;
    while (1)
    {
        LED_On();
        delay_busy(20000000);
        LED_Off();
        delay_busy(20000000);

        UART1_SendString("[ECC] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}
