# bootloader 컴포넌트 부모 컨테이너 — Secure Boot 는 2-stage 구성.
#
#   Stage 1 (immutable, 24KB)  검증자(verifier). vector sanity → magic → size
#                              → CRC32 → SHA-256 → RSA-2048 서명 검증.
#   Stage 2 (256KB)            검증 대상. 호스트가 서명해 첨부한 image.
#                              Stage 1 이 검증 통과 시 점프, 실패 시 halt.
add_subdirectory(stage1)
add_subdirectory(stage2)
