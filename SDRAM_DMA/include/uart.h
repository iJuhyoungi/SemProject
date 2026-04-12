#ifndef UART_H
#define UART_H

#include <stdint.h>

__attribute__((section(".boot_text")))void UART1_Init(void);
__attribute__((section(".boot_text")))void UART1_SendChar(char c);
__attribute__((section(".boot_text")))void UART1_SendString(const char *str);
__attribute__((section(".boot_text")))void UART1_SendHex32(uint32_t val);

#endif // UART_H
