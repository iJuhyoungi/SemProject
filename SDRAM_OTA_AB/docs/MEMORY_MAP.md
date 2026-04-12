# Memory Map Notes

현재 링커 스크립트는 RT1020의 일반적인 FlexRAM 분할을 기준으로 작성되었습니다.

## 섹션 배치

- `.flash_config` -> `0x60000000`
- `.boot_header` -> `0x60001000`
- `.isr_vector`, `.text`, `.rodata` -> `0x60002000` 이후
- `.data`, `.bss` -> `DTCM`
- `heap`, `stack` -> `OCRAM`

## 수정이 필요한 경우

- FlexRAM을 `128 KB OCRAM / 64 KB ITCM / 64 KB DTCM` 외 다른 값으로 나누는 경우
- 실행 위치를 SRAM으로 바꾸는 경우
- 외부 플래시 크기가 8 MB가 아닌 경우
- 부트 헤더에 DCD/CSF가 필요한 경우
