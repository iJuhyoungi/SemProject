#include "Fls.h"

/* F-4 에서 하드코딩했던 실험 영역을 config 로. 이미지(0x0~)는 이 밖이라 거부된다. */
const Fls_ConfigType Fls_Config =
{
    .baseAddress   = 0x00700000u,   /* flash 뒤쪽 1MB */
    .totalSize     = 0x00100000u,
    .sectorSize    = 4096u,
    .pageSize      = 256u,
    .maxWriteChunk = 8u,            /* TX FIFO 워터마크(8B) 단위. 더 큰 write 는 이 크기로 쪼개 프로그램한다 */
};
