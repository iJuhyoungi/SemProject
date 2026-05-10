#ifndef UART_H
#define UART_H

#include <stdint.h>

__attribute__((section(".boot_text")))void UART1_Init(void);
__attribute__((section(".boot_text")))void UART1_SendChar(char c);
__attribute__((section(".boot_text")))void UART1_SendString(const char *str);
__attribute__((section(".boot_text")))void UART1_SendHex32(uint32_t val);

/* RX (Phase 4 active recovery 에서 처음 사용 — 다른 binary 도 reuse 가능) */                                                                                                            
__attribute__((section(".boot_text")))uint8_t UART1_RxByte(void);                                                                                                                        
__attribute__((section(".boot_text")))void    UART1_RxBytes(uint8_t *buf, uint32_t n); 

#endif // UART_H
