#include "Det.h"
#include "uart.h"

Std_ReturnType Det_ReportError(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId, uint8_t ErrorId)
{
    (void)InstanceId;
    UART1_SendString("[Det] mod=");
    UART1_SendHex32(ModuleId);
    UART1_SendString(" api=");
    UART1_SendHex32(ApiId);
    UART1_SendString(" err=");
    UART1_SendHex32(ErrorId);
    UART1_SendString("\r\n");
    return E_OK;
}
