#include <stddef.h>
#include <stdint.h>

void *memset(void *dst, int val, size_t n)
{
    uint8_t *d = (uint8_t*)dst;
    for(size_t i=0;i<n;++i){
        d[i]=(uint8_t)val;
    }
    return dst;

}

void *memcpy(void *dst, const void *src, size_t n){
    uint8_t *d=(uint8_t*)dst;
    const uint8_t *s=(const uint8_t*)src;
    for(size_t i=0;i<n;++i){
        d[i]=s[i];
    }
    return dst;
}