#include "Ecc72.h"

static uint8_t g_col[64];

static uint8_t popcount8(uint8_t v)
{
    uint8_t c=0;
    while(v!=0u){
        c++;
        v&=(uint8_t)(v-1u);
    }
    return c;
}

static uint8_t parity8(uint8_t v)
{
    v^=(uint8_t)(v>>4);
    v^=(uint8_t)(v>>2);
    v^=(uint8_t)(v>>1);
    return (uint8_t)(v&1u);
}

void Ecc72_Init(void)
{
    uint16_t v;
    uint8_t n=0u;

    for(v=0u;(v<256u)&&(n<64);++v){
        uint8_t b=(uint8_t)v;
        uint8_t w=popcount8(b);
        if((w>=3u)&&((w&1)==1)){
            g_col[n++]=b;
        }
    }
}

uint8_t Ecc72_Encode(uint64_t data)
{
    uint8_t ecc=0u;
    uint8_t i;
    for(i=0;i<64;++i){
        if((data>>i)&1){
            ecc^=g_col[i];
        }
    }
    return ecc;
}

Ecc_Status Ecc72_Decode(uint64_t *data, uint8_t ecc)
{
    uint8_t syn=(uint8_t)(Ecc72_Encode(*data)^ecc);
    uint8_t i;

    if(syn==0){
        return ECC_NO_ERROR;
    }

    if(parity8(syn)==1){
        for(i=0;i<64;++i){
            if(g_col[i]==syn){
                *data^=((uint64_t)1u<<i);
                return ECC_CORRECTED;
            }
        }

        if(popcount8(syn)==1){
            return ECC_CORRECTED;
        }

        return ECC_UNCORRECTABLE;
    }

    return ECC_UNCORRECTABLE;

}
