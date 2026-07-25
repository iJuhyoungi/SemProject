#include "Fee.h"

/* 논리 블록 정의. 상위 스택은 이 블록 번호로 저장/조회한다.
 * 크기는 FlexSPI_ReadData 한 번(<=32B)에 들어오도록 작게 잡았다 (F-6a/b 범위). */
const Fee_BlockConfigType Fee_BlockConfig[] =
{
    { .blockNumber = 1u, .blockSize = 8u },
    { .blockNumber = 2u, .blockSize = 16u },
};

const uint16_t Fee_NumBlocks = (uint16_t)(sizeof(Fee_BlockConfig) / sizeof(Fee_BlockConfig[0]));
