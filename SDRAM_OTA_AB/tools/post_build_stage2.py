#!/usr/bin/env python3
"""
Stage 2 의 .bin 파일 후처리:
- vector[7] (offset 0x1C) 에 MAGIC (0xDEADBEEF) write                                                                                                                                    
- vector[8] (offset 0x20) 에 binary size write                                                                                                                                           
- vector[9] (offset 0x24) 에 CRC32 write                                                                                                                                                 
                                                                                                                                                                                        
CRC32 는 vector[9] 자리를 0 으로 둔 상태에서 binary 전체에 대해 계산.
Stage 1 이 검증 시 동일 방법 적용.                                                                                                                                                       
"""                                                                                                                                                                                      
                                                                                                                                                                                        
import sys                                                                                                                                                                               
import struct                                             
import zlib

MAGIC_OFFSET = 0x1C   # vector[7]                                                                                                                                                        
SIZE_OFFSET  = 0x20   # vector[8]
CRC_OFFSET   = 0x24   # vector[9]                                                                                                                                                        
                                                                                                                                                                                        
MAGIC_VALUE  = 0xDEADBEEF
                                                                                                                                                                                        
                                                                                                                                                                                        
def patch(bin_path: str) -> None:
    # 1. read                                                                                                                                                                            
    with open(bin_path, "rb") as f:                       
        data = bytearray(f.read())
                                                                                                                                                                                        
    size = len(data)
    if size <= CRC_OFFSET + 4:                                                                                                                                                           
        raise SystemExit(f"Binary too small ({size} bytes), need > {CRC_OFFSET + 4}")                                                                                                    
                                                                                                                                                                                        
    # 2. magic + size 채우기                                                                                                                                                             
    struct.pack_into("<I", data, MAGIC_OFFSET, MAGIC_VALUE)                                                                                                                              
    struct.pack_into("<I", data, SIZE_OFFSET, size)                                                                                                                                      
                                                                                                                                                                                        
    # 3. CRC 자리는 0 으로 두고 CRC32 계산                                                                                                                                               
    struct.pack_into("<I", data, CRC_OFFSET, 0)                                                                                                                                          
    crc = zlib.crc32(data) & 0xFFFFFFFF                                                                                                                                                  
                                                                                                                                                                                        
    # 4. CRC write
    struct.pack_into("<I", data, CRC_OFFSET, crc)                                                                                                                                        
                                                                                                                                                                                        
    # 5. write back
    with open(bin_path, "wb") as f:                                                                                                                                                      
        f.write(data)                                     

    print(f"  [post_build] size=0x{size:08x}, magic=0x{MAGIC_VALUE:08x}, crc=0x{crc:08x}")                                                                                               

                                                                                                                                                                                        
if __name__ == "__main__":                                
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} <stage2.bin>")
    patch(sys.argv[1])

