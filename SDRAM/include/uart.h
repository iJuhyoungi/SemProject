#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART1_Init(void);
void UART1_SendChar(char c);
void UART1_SendString(const char *str);
void UART1_SendHex32(uint32_t val);

#endif // UART_H
