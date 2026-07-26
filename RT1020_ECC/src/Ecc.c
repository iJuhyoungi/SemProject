#include "Ecc.h"

/* 각 패리티가 커버하는 위치의 마스크 (위치 번호를 2진수로 봤을 때 그 비트가 1인 자리). */
#define ECC_M1 ((1u << 1) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11))
#define ECC_M2 ((1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 10) | (1u << 11))
#define ECC_M4 ((1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 12))
#define ECC_M8 ((1u << 8) | (1u << 9) | (1u << 10) | (1u << 11) | (1u << 12))

static uint8_t xor_bits(uint16_t v)
{
    v ^= (uint16_t)(v >> 8);
    v ^= (uint16_t)(v >> 4);
    v ^= (uint16_t)(v >> 2);
    v ^= (uint16_t)(v >> 1);
    return (uint8_t)(v & 1u);
}

/* 데이터 8비트를 코드워드의 데이터 위치(3,5,6,7,9,10,11,12)에 적용 */
static uint16_t place_data(uint8_t data)
{
    uint16_t cw=0u;
    if (data & 0x01u) cw |= (1u << 3);   /* d1 -> 위치 3  */
    if (data & 0x02u) cw |= (1u << 5);   /* d2 -> 위치 5  */
    if (data & 0x04u) cw |= (1u << 6);   /* d3 -> 위치 6  */
    if (data & 0x08u) cw |= (1u << 7);   /* d4 -> 위치 7  */
    if (data & 0x10u) cw |= (1u << 9);   /* d5 -> 위치 9  */
    if (data & 0x20u) cw |= (1u << 10);  /* d6 -> 위치 10 */
    if (data & 0x40u) cw |= (1u << 11);  /* d7 -> 위치 11 */
    if (data & 0x80u) cw |= (1u << 12);  /* d8 -> 위치 12 */
    return cw;
}

/* 코드워드의 데이터 위치에서 8비트 데이터를 다시 모은다. */
static uint8_t extract_data(uint16_t cw)
{
    uint8_t d = 0u;
    if (cw & (1u << 3))  d |= 0x01u;
    if (cw & (1u << 5))  d |= 0x02u;
    if (cw & (1u << 6))  d |= 0x04u;
    if (cw & (1u << 7))  d |= 0x08u;
    if (cw & (1u << 9))  d |= 0x10u;
    if (cw & (1u << 10)) d |= 0x20u;
    if (cw & (1u << 11)) d |= 0x40u;
    if (cw & (1u << 12)) d |= 0x80u;
    return d;
}

uint16_t Ecc_Encode(uint8_t data)
{
    uint16_t cw=place_data(data);

    /* Hamming 패리티: 각 커버 집합의 1 개수를 짝수로 맞춘다. */
    if (xor_bits(cw & ECC_M1)) cw |= (1u << 1);
    if (xor_bits(cw & ECC_M2)) cw |= (1u << 2);
    if (xor_bits(cw & ECC_M4)) cw |= (1u << 4);
    if (xor_bits(cw & ECC_M8)) cw |= (1u << 8);

    if(xor_bits(cw)) cw|=(1u<<0);

    return cw;
}

Ecc_Status Ecc_Decode(uint16_t cw, uint8_t *dataOut)
{
    uint8_t s1 = xor_bits(cw & ECC_M1);
    uint8_t s2 = xor_bits(cw & ECC_M2);
    uint8_t s4 = xor_bits(cw & ECC_M4);
    uint8_t s8 = xor_bits(cw & ECC_M8);
    uint8_t S  = (uint8_t)(s1 | (s2 << 1) | (s4 << 2) | (s8 << 3));

    /* 전체 패리티 검사 P = 코드워드 전체(bit0..12) XOR. 홀수 개 오류면 1. */
    uint8_t P=xor_bits(cw);
    Ecc_Status st;
    if((S==0u)&&(P==0u)){
        st=ECC_NO_ERROR;
    }else if(P==1u){
        /* 홀수 개 = 1비트 오류. S 위치를 뒤집어 정정한다.
        * S==0 이면 전체패리티 비트(p0=bit0) 자체가 틀린 것이라 bit0 을 정정 -> 데이터는 이미 정상. */
        cw^=(uint16_t)(1u<<S);
        st=ECC_CORRECTED;
    }else{
        st=ECC_UNCORRECTABLE;
    }

    *dataOut=extract_data(cw);
    return st;
}
