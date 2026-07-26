#ifndef CRC32_H
#define CRC32_H                                                                                                                                                                          

#include <stdint.h>                                                                                                                                                                      
                                                        
/*
* IEEE 802.3 CRC-32 (zlib.crc32 호환).
* Polynomial: 0xEDB88320 (reflected)
* Init: 0xFFFFFFFF, Final XOR: 0xFFFFFFFF                                                                                                                                               
*/
uint32_t CRC32_Compute(const uint8_t *data, uint32_t len);                                                                                                                               
                                                                                                                                                                                        
/*
* 메모리 영역에 대해 CRC 계산하되, [skip_offset, skip_offset+4) 영역은                                                                                                                  
* 0 byte 로 가정 (image header 의 CRC 자리 self-reference 처리용).                                                                                                                      
*
* post_build_stage2.py 와 동일 알고리즘 — 두 곳의 결과가 일치해야 검증 통과.                                                                                                            
*/                                                                                                                                                                                      
uint32_t CRC32_ComputeWithSkip(const uint8_t *data, uint32_t len, uint32_t skip_offset);                                                                                                 
                                                                                                                                                                                        
#endif