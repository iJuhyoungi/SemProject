#ifndef DET_H
#define DET_H

#include <stdint.h>
#include "Std_Types.h"

/* 개발 단계 오류 신고. 실차에서는 로깅/트랩으로, 여기서는 UART 로 찍는다. */
Std_ReturnType Det_ReportError(uint16_t ModuleId, uint8_t InstanceId, uint8_t ApiId, uint8_t ErrorId);

#endif /* DET_H */