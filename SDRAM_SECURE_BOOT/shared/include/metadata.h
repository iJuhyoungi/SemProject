#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

#define METADATA_MAGIC          0x5EC8B007u             // "SECBOOT" 비트변형
#define METADATA_PRIMARY_BASE   0x600C8000u             // primary app base 주소
#define METADATA_BACKUP_BASE    0x600C9000u             // backup app base 주소
#define METADATA_HEADER_SIZE    0x20u                   // magic + seq + min_ver + reserved (= SHA 입력)

/* metadata header layout (flash의 첫 64 byte) */
typedef struct{
    uint32_t magic;                     // 0x00
    uint32_t sequence_number;           // 0x04
    uint32_t min_acceptable_version;    // 0x08
    uint32_t reserved[5];               // 0x0C ~ 0x1F
    uint8_t sha256[32];                 // 0x20 ~ 0x3F [0x00 .. 0x1F]까지의 SHA256 해시값
}metadata_t;

/**
 * dual metadata를 읽고 valid한 것 중 큰 sequence_number를 선택
 * 성공 시 *out에 1을 채우고, 둘다 invalid하면 0을 채움
 */

int metadata_read_active(metadata_t *out);


#endif /* METADATA_H */
