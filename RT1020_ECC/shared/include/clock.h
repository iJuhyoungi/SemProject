#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

extern volatile uint32_t g_clock_init_marker;

void Clock_Init_132MHz(void);

#endif // CLOCK_H
