#include "verify.h"
#include "uart.h"
#include "crc32.h"
#include "sha256.h"
#include "rsa.h"
#include "rt1020_regs.h"

static void print_digest_hex(const uint8_t digest[SHA256_DIGEST_SIZE])
{
    static const char hex[]="0123456789abcdef";
    char buf[3];
    buf[2]='\0';

    for(uint32_t i=0;i<SHA256_DIGEST_SIZE;++i){
        buf[0]=hex[digest[i]>>4];
        buf[1]=hex[digest[i]&0x0fu];
        UART1_SendString(buf);
    }
    UART1_SendString("\r\n");
}

static int vector_sane(uint32_t addr)
{
    uint32_t sp=*(volatile uint32_t*)addr;
    uint32_t pc=*(volatile uint32_t*)(addr+4);          
    if((sp&0xF0000000u)!=0x20000000u)return 0;              /* SP in DTCM/OCRAM */
    if((pc&0xF0000000u)!=0x60000000u)return 0;              /* PC FlexSPI Flash */
    if((pc&0x1u)!=0x1u)return 0;                            /* Thumb bit */
    return 1;
}

int verify_image(uint32_t base, const bn_t modulus){
    if(!vector_sane(base)){
        UART1_SendString("[Verify] Vector sanity check failed\r\n");
        return 0;
    }

    uint32_t magic=*(volatile uint32_t*)(base+IMG_MAGIC_OFFSET);
    if(magic!=IMG_MAGIC_VALUE){
        UART1_SendString("[Verify] Magic check failed\r\n");
        return 0;
    }

    uint32_t size=*(volatile uint32_t*)(base+IMG_SIZE_OFFSET);
    if(size<IMG_SIZE_MIN || size>IMG_SIZE_MAX){
        UART1_SendString("[Verify] Size check failed\r\n");
        return 0;
    }

    uint32_t expected_crc=*(volatile uint32_t*)(base+IMG_CRC_OFFSET);
    uint32_t computed_crc=CRC32_ComputeWithSkip(
        (const uint8_t*)base, size, IMG_CRC_OFFSET
    );
    if(computed_crc!=expected_crc){
        UART1_SendString("[Verify] CRC32 FAIL\r\n");
        return 0;
    }

    uint8_t img_hash[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t*)base, size, img_hash);
    UART1_SendString("[Verify] SHA-256: ");
    print_digest_hex(img_hash);

    const uint8_t *signature=(const uint8_t*)(base+size);
    if(!rsa_verify_pkcs1_v15_sha256(img_hash, signature, modulus)){
        UART1_SendString("[Verify] RSA signature FAIL\r\n");
        return 0;
    }

    UART1_SendString("[Verify] RSA signature OK\r\n");
    return 1;
}

void jump_to_image(uint32_t addr){
    uint32_t app_msp=*(volatile uint32_t *)addr;
    uint32_t app_pc = *(volatile uint32_t*)(addr+4);

    __asm volatile("cpsid i");
    __asm volatile("dsb");
    __asm volatile("isb");

    SCB_VTOR = addr;

    __asm volatile("dsb");
    __asm volatile("isb");

    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :: "r"(app_msp), "r"(app_pc));
}