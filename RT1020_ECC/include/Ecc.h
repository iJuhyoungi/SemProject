#ifndef ECC_H
#define ECC_H

#include <stdint.h>

typedef enum
{
    ECC_NO_ERROR=0,
    ECC_CORRECTED,
    ECC_UNCORRECTABLE
} Ecc_Status;

uint16_t Ecc_Enable(uint8_t data);
uint16_t   Ecc_Encode(uint8_t data);
Ecc_Status Ecc_Decode(uint16_t codeword, uint8_t *dataOut);

#endif
