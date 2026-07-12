#ifndef PORT_H
#define PORT_H

void Port_Init(void);
void Port_RoutePwmTrigToQtmr(void);   /* XBAR: PWM SM0 트리거 -> QTIMER1_TIMER2 입력 */

#endif
