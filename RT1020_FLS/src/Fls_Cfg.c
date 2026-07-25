#include "Fls.h"

/* F-4 에서 하드코딩했던 실험 영역을 config 로. 이미지(0x0~)는 이 밖이라 거부된다. */
const Fls_ConfigType Fls_Config =
{
    .baseAddress   = 0x00700000u,   /* flash 뒤쪽 1MB */
    .totalSize     = 0x00100000u,
    .sectorSize    = 4096u,
    .pageSize      = 256u,
    .maxWriteChunk = 32u,           /* FLS_IP_READ_MAX 와 동일 (한 번의 TX FIFO 채움) */
};
