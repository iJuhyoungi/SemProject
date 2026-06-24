#ifndef CAN_H
#define CAN_H

#include <stdint.h>
int Can1_Init(void); // loopback 모드 initialize(0=OK, 1=timeout)
int Can1_LoopbackTest(void);            // 프레임 1개 TX -> loopback RX -> 데이터 일치 확인

#endif