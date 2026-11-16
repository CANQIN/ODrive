#include "cubemx_hooks.h"
#include <stdbool.h>

__attribute__((used))
void get_regs(void** stack_ptr) {
  odrive_fault_shutdown();

  void* volatile r0 __attribute__((unused)) = stack_ptr[0];
  void* volatile r1 __attribute__((unused)) = stack_ptr[1];
  void* volatile r2 __attribute__((unused)) = stack_ptr[2];
  void* volatile r3 __attribute__((unused)) = stack_ptr[3];

  void* volatile r12 __attribute__((unused)) = stack_ptr[4];
  void* volatile lr __attribute__((unused)) = stack_ptr[5];  // Link register
  void* volatile pc __attribute__((unused)) = stack_ptr[6];  // Program counter
  void* volatile psr __attribute__((unused)) = stack_ptr[7];  // Program status register

  void* volatile cfsr __attribute__((unused)) = (void*)SCB->CFSR; // Configurable fault status register
  void* volatile cpacr __attribute__((unused)) = (void*)SCB->CPACR;
  void* volatile fpccr __attribute__((unused)) = (void*)FPU->FPCCR;

  volatile bool preciserr __attribute__((unused)) = (uint32_t)cfsr & 0x200;
  volatile bool ibuserr __attribute__((unused)) = (uint32_t)cfsr & 0x100;

  volatile int stay_looping = 1;
  while(stay_looping);
}

/**
* @brief This function handles Hard fault interrupt.
*/
__attribute__((naked))
void HardFault_Handler(void)
{
  __asm(
    " tst lr, #4     \n\t"
    " ite eq         \n\t"
    " mrseq r0, msp  \n\t"
    " mrsne r0, psp  \n\t"
    " b get_regs     \n\t"
  );
}
