#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>
#include "secure.h"

#define METADATA_MAGIC 0x5EC8B007u        // "SECBOOT" 비트변형
#define METADATA_PRIMARY_BASE 0x600C8000u // primary app base 주소
#define METADATA_BACKUP_BASE 0x600C9000u  // backup app base 주소
#define METADATA_HEADER_SIZE 0x20u        // magic + seq + min_ver + reserved (= SHA 입력)
#define METADATA_SIG_OFFSET 0x20u         // RSA-2048 signature 시작
#define METADATA_SIG_SIZE 256u            // RSA-2048 signature 크기

/* metadata header layout (flash의 첫 64 byte) */
typedef struct
{
    uint32_t magic;                  // 0x00
    uint32_t sequence_number;        // 0x04
    uint32_t min_acceptable_version; // 0x08
    uint32_t reserved[5];            // 0x0C ~ 0x1F
    uint8_t signature[256];          // 0x20 ~ 0x11F (RSA-2048)
} metadata_t;

typedef enum
{
    METADATA_OK = 0,
    METADATA_BAD_MAGIC = 1,
    METADATA_BAD_SIGNATURE = 2,
} metadata_reason_t;

/**
 * Dual metadata 를 읽고 valid 한 것 중 큰 sequence_number 채택.
 *
 * reason 은 UART 진단용 정보일 뿐 보안 판정의 근거가 아닙니다. 판정은 반환값이
 * 담고 있고, 반드시 SEC_IS_PASS() 로 확인해야 합니다.
 *
 * @param out             결과 metadata (성공 시 채워짐)
 * @param primary_reason  primary 검증 reason (NULL 허용)
 * @param backup_reason   backup 검증 reason (NULL 허용)
 * @return SEC_PASS if 최소 한쪽 valid, SEC_FAIL if 둘 다 invalid
 */

sec_bool_t metadata_read_active(metadata_t *out, metadata_reason_t *primary_reason, metadata_reason_t *backup_reason);

#endif /* METADATA_H */
