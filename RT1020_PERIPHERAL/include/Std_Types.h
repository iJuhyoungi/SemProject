#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

typedef uint8_t Std_ReturnType;

#define E_OK            ((Std_ReturnType)0x00u)
#define E_NOT_OK        ((Std_ReturnType)0x01u)

#define STD_ON          0x01u
#define STD_OFF         0x00u
#define STD_HIGH        0x01u
#define STD_LOW         0x00u

#endif      /* STD_TYPES_H */