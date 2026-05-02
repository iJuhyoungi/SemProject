#ifndef BOOTCTRL_RUNTIME_BOOT_H
#define BOOTCTRL_RUNTIME_BOOT_H

#include <stdint.h>
#include "boot_policy.h"

  /*                                                        
   * Trial slot 으로 점프하기 직전, bootloader 가 호출.
   *                                                                                                                                                                                       
   * current: 현재 boot_ctrl 영역에서 읽은 metadata (Boot_GetCtrl() 결과)
   *                                                                                                                                                                                       
   * 동작: current.boot_attempts 를 +1 한 새 image 를       
   *       boot_ctrl sector(0x603F0000) 에 erase + program + verify.                                                                                                                       
   *                                                                                                                                                                                       
   * 반환: 성공 시 1, 실패 시 0.                                                                                                                                                           
   */  

int BootCtrl_MarkTrialAttempt(const boot_ctrl_t *current);



#endif