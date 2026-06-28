#ifndef ICU_H
#define ICU_H

#include <stdint.h>

void Icu1_Init(void);               //ch0 사각파 생성 + ch1 cascade 카운트 시작
uint16_t Icu1_GetEdgeCount(void);   // ch1 CNTR = ch0 누적 Edge 수

#endif
