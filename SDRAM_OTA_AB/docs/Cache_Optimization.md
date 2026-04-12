# NXP i.MX RT1020 Bare-Metal Firmware Project

## 🚀 Phase 1.4: MPU & Cache Optimization

본 단계의 목표는 SDRAM으로 성공적으로 릴로케이션(Relocation)된 펌웨어가 느린 외부 버스의 병목을 겪지 않고, 132MHz 코어 클럭의 퍼포먼스를 100% 낼 수 있도록 메모리 아키텍처를 최적화하는 것입니다.

### [B-1] MPU (Memory Protection Unit) 설정 및 영역 분리
Cortex-M7 코어에게 각 메모리 물리 주소의 특성을 명확히 지정하여, 안전한 캐싱(Caching)과 주변장치 제어의 토대를 마련했습니다.

* **Region 0 (Flash / XIP):** `0x60000000` (8MB)
  * **속성:** Normal Memory, Cacheable, Execute Allow
  * **목적:** 부팅 초기화 코드 및 Read-Only 데이터(문자열 상수 등)의 빠른 접근 허용.
* **Region 1 (SDRAM):** `0x80000000` (32MB)
  * **속성:** Normal Memory, Cacheable (Write-Back, Write-Allocate), Execute Allow
  * **목적:** 메인 펌웨어의 실행(Virtual Memory) 및 대용량 데이터 처리 공간.
* **Region 2 (Peripherals):** `0x40000000` (512MB)
  * **속성:** Device Memory, Non-Cacheable, Execute Never (XN)
  * **목적:** 하드웨어 레지스터 제어 시 캐시 불일치(Coherency)로 인한 오작동 방지 및 실수로 인한 코드 실행 원천 차단.

**🔥 트러블슈팅 (Debugger Lock-up 방어):**
MPU 설정 함수(`MPU_Init`)가 SDRAM 켜지기 전인 부팅 극초반(`SystemInit`)에 호출됨에도 불구하고, 해당 함수가 SDRAM(`.text`) 영역으로 컴파일되어 JTAG/SWD 디버거마저 접속이 끊기는 치명적인 Lock-up(하드폴트) 현상 발생. 
➔ **해결:** 링커 스크립트(`.ld`)의 `.boot_text` 섹션에 `*mpu*(.text*)`를 추가하여 MPU 초기화 모듈을 통째로 Flash에 강제 고정함으로써 부트 시퀀스 안정성 완벽 확보.

### [B-2] I-Cache (Instruction Cache) 활성화
명령어(Instruction)는 실행 중 값이 변하지 않는 읽기 전용(Read-Only) 속성을 가지므로, 데이터 불일치 이슈 없이 즉각적인 성능 향상을 얻을 수 있습니다.

* **구현 내용:** SCB (System Control Block) 레지스터를 직접 제어하여 코어 내부의 I-Cache 엔진을 점화.
  ```c
  void ICache_Enable(void)
  {
      if (SCB_CCR & SCB_CCR_IC_Msk) return;

      __asm("dsb");
      __asm("isb");

      /* 캐시 내부 쓰레기 값 클리어 */
      SCB_ICIALLU = 0UL; 
      /* I-Cache 활성화 */
      SCB_CCR |= SCB_CCR_IC_Msk; 

      __asm("dsb");
      __asm("isb");
  }
  ```
* **결과:** 부팅 직후 `main` 함수 진입과 동시에 I-Cache를 활성화하여, SDRAM 무결성 가혹 테스트(`SDRAM_Test16`, `SDRAM_Test32`) 루프 진입 시 코어 내부 SRAM에서 명령어를 즉각 페치(Fetch)함으로써 비약적인 실행 속도 향상 달성.

---

### 🔜 Next Steps
* **[B-3] D-Cache (Data Cache) Enable:** 데이터 캐시를 활성화하고, 순수 CPU 워크로드(SDRAM memcpy, 배열 순회 등) 기반의 벤치마크 수행.
* **[B-4] DMA Coherency Management:** 향후 주변장치(SPI, ADC) 연동을 대비하여, DMA 전용 Non-cacheable 버퍼 설계 및 Cache Clean/Invalidate 정책 도입.
