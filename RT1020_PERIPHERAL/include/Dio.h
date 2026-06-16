#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"
typedef uint8_t Dio_ChannelType;
typedef uint8_t Dio_LevelType;                                          /* STD_HIGH / STD_LOW */

/* 레벨1: 단일 채널 = 보드 LED. 실무 Dio 는 채널/포트 테이블 */
void Dio_Init(void);
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

#endif
