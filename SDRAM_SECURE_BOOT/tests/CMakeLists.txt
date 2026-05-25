cmake_minimum_required(VERSION 3.16)
project(secure_boot_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O0 -g")

# Unity (C UT framework)
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# 테스트 대상 — 보드 shared 코드를 host gcc 로 그대로 빌드
add_library(bignum STATIC ../shared/src/bignum.c)
target_include_directories(bignum PUBLIC ../shared/include)

# 테스트 실행 파일
add_executable(test_bignum src/test_bignum.c)
target_link_libraries(test_bignum PRIVATE unity bignum)

# 기존 bignum 등록 다음에 추가
add_library(sha256 STATIC ../shared/src/sha256.c)
target_include_directories(sha256 PUBLIC ../shared/include)

add_executable(test_sha256 src/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE unity sha256)

add_test(NAME sha256 COMMAND test_sha256)

add_library(rsa STATIC ../shared/src/rsa.c)
target_include_directories(rsa PUBLIC ../shared/include)
target_link_libraries(rsa PUBLIC bignum sha256)

add_executable(test_rsa src/test_rsa.c)
target_link_libraries(test_rsa PRIVATE unity rsa)

add_test(NAME rsa COMMAND test_rsa)

# CTest 통합
enable_testing()
add_test(NAME bignum COMMAND test_bignum)


