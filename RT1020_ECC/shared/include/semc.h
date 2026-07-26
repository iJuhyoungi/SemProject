#ifndef SEMC_H
#define SEMC_H

void SDRAM_Pin_Init(void);
void SDRAM_Clock_Init(void);
int SDRAM_Init_Sequence(void);
int SDRAM_Test16(void);
int SDRAM_Test32(void);
#endif // SEMC_H
