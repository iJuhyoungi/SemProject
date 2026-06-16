#include "Port.h"
#include "lpspi.h"

/* LPSPI1 핀(SCK/PCS0/SDO/SDI) IOMUXC mux. 실무 Port 는 전체 핀 config 테이블 적용 */
void Port_Init(void)
{
    LPSPI1_Clock_Enable();
}
