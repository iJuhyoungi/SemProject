# NXP i.MX RT1020 Bare-Metal Firmware Project

## 🚀 Phase 1.4: MPU & Cache Optimization (Performance Unleashed)

본 단계의 목표는 SDRAM으로 성공적으로 릴로케이션(Relocation)된 펌웨어가 느린 외부 버스의 병목을 겪지 않고, 132MHz 코어 클럭의 퍼포먼스를 100% 낼 수 있도록 메모리 아키텍처를 최적화하는 것입니다. HAL/CMSIS 라이브러리를 철저히 배제하고 레지스터 직접 제어를 통해 구현했습니다.

---

### [B-1] MPU (Memory Protection Unit) 설정 및 영역 분리
Cortex-M7 코어에게 각 메모리 물리 주소의 특성을 명확히 지정하여, 안전한 캐싱(Caching)과 주변장치 제어의 토대를 마련했습니다.

* **Region 0 (Flash / XIP):** `0x60000000` (8MB)
  * **속성:** Normal Memory, Cacheable, Execute Allow
* **Region 1 (SDRAM):** `0x80000000` (32MB)
  * **속성:** Normal Memory, Cacheable (Write-Back, Write-Allocate), Execute Allow
  * **목적:** 메인 펌웨어의 고속 실행 및 대용량 데이터 처리 공간.
* **Region 2 (Peripherals):** `0x40000000` (512MB)
  * **속성:** Device Memory, Non-Cacheable, Execute Never (XN)
  * **목적:** 하드웨어 레지스터 제어 시 캐시 불일치(Coherency)로 인한 오작동 방지 및 실수로 인한 코드 실행 원천 차단.

**🔥 트러블슈팅 (Debugger Lock-up 방어):**
* **이슈:** MPU 설정 함수(`MPU_Init`)가 SDRAM 활성화 이전(`SystemInit`)에 호출됨에도 불구하고, 해당 함수가 SDRAM(`.text`) 영역으로 배치되어 JTAG/SWD 디버거마저 접속이 끊기는 치명적인 Lock-up(BusFault) 현상 발생.
* **해결:** 링커 스크립트(`.ld`)의 `.boot_text` 섹션에 `*mpu*(.text*)`를 추가하여 MPU 초기화 모듈을 통째로 Flash에 강제 고정함으로써 부트 시퀀스 안정성 완벽 확보.

---

### [B-2] I-Cache (Instruction Cache) 활성화
명령어(Instruction)는 실행 중 값이 변하지 않는 읽기 전용(Read-Only) 속성을 가지므로, 데이터 불일치 이슈 없이 즉각적인 성능 향상을 얻을 수 있습니다. SCB (System Control Block) 레지스터의 `ICIALLU`를 통해 캐시를 안전하게 비운 뒤 활성화하여, 루프 실행 시 코어 내부 SRAM에서 명령어를 즉각 페치(Fetch)하도록 구성했습니다.

---

### [B-3] D-Cache (Data Cache) 활성화 및 하드웨어 벤치마크
D-Cache는 I-Cache와 달리 활성화 전 하드웨어의 Set과 Way를 순회하며 무효화(Invalidate)하는 과정이 필수적입니다. 외부 라이브러리 없이 `CCSIDR`, `DCISW` 레지스터를 직접 타격하는 순수 베어메탈 제어 코드를 작성하여 D-Cache 엔진을 점화했습니다.

**📊 256KB SDRAM 메모리 복사 벤치마크 결과**
ARM Cortex-M7 내부의 DWT 사이클 카운터(`DWT_CYCCNT`)를 활용하여, 132MHz 클럭 기준 256KB 데이터를 SDRAM 내부에서 복사하는 데 소요된 정확한 하드웨어 사이클을 측정했습니다.

| 상태 | 사이클 수 (16진수) | 사이클 수 (10진수) | 132MHz 기준 소요 시간 |
| :--- | :--- | :--- | :--- |
| **D-Cache OFF** | `0x00907242` | **9,466,434** 사이클 | 약 **71.7 ms** |
| **D-Cache ON** | `0x0010E057` | **1,106,007** 사이클 | 약 **8.3 ms** |
| **성능 향상** | - | - | 🚀 **약 8.56배 (856%) 속도 폭발!** |

**💡 아키텍처 분석:**
약 8.5배의 극적인 성능 향상은 MPU에서 지정한 `Write-Back, Write-Allocate` 정책 덕분입니다. 캐시 힛(Cache Hit) 시 외부 버스 접근 없이 0 사이클 단위로 데이터가 처리되며, 32바이트 단위의 캐시 라인(Cache Line) 버스트 읽기/쓰기가 극한의 퍼포먼스를 만들어냈습니다.

---

### 🔜 Next Target: [Option C] DMA & Cache Coherency
메모리와 코어의 성능 최적화가 완료되었습니다. 다음 단계에서는 CPU 개입 없이 메모리 간 데이터를 이동시키는 DMA(Direct Memory Access)를 도입합니다. D-Cache 활성화로 인해 필연적으로 발생하는 '캐시 불일치(Cache Coherency)' 문제를 Non-cacheable 메모리 영역 분리 및 Cache Clean/Invalidate 정책을 통해 해결할 예정입니다.
