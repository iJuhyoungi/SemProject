#ifndef ECC72_H
#define ECC72_H

#include <stdint.h>
#include "Ecc.h"

void Ecc72_Init(void);
uint8_t Ecc72_Encode(uint64_t data);
Ecc_Status Ecc72_Decode(uint64_t *data, uint8_t ecc);

#endif
