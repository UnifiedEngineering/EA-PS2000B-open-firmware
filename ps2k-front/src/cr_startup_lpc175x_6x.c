#include "chip.h"

#define WEAK __attribute__ ((weak))
#define ALIAS(f) __attribute__ ((weak, alias (#f)))

// Declaration of external SystemInit function
extern void SystemInit(void);

WEAK void IntDefaultHandler(void);

void ResetISR(void);
void NMI_Handler(void) ALIAS(IntDefaultHandler);
void HardFault_Handler(void) ALIAS(IntDefaultHandler);
void MemManage_Handler(void) ALIAS(IntDefaultHandler);
void BusFault_Handler(void) ALIAS(IntDefaultHandler);
void UsageFault_Handler(void) ALIAS(IntDefaultHandler);
void SVC_Handler(void) ALIAS(IntDefaultHandler);
void DebugMon_Handler(void) ALIAS(IntDefaultHandler);
void PendSV_Handler(void) ALIAS(IntDefaultHandler);
void SysTick_Handler(void) ALIAS(IntDefaultHandler);

void WDT_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER0_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER1_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER2_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER3_IRQHandler(void) ALIAS(IntDefaultHandler);
void UART0_IRQHandler(void) ALIAS(IntDefaultHandler);
void UART1_IRQHandler(void) ALIAS(IntDefaultHandler);
void UART2_IRQHandler(void) ALIAS(IntDefaultHandler);
void UART3_IRQHandler(void) ALIAS(IntDefaultHandler);
void PWM1_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2C0_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2C1_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2C2_IRQHandler(void) ALIAS(IntDefaultHandler);
void SPI_IRQHandler(void) ALIAS(IntDefaultHandler);
void SSP0_IRQHandler(void) ALIAS(IntDefaultHandler);
void SSP1_IRQHandler(void) ALIAS(IntDefaultHandler);
void PLL0_IRQHandler(void) ALIAS(IntDefaultHandler);
void RTC_IRQHandler(void) ALIAS(IntDefaultHandler);
void EINT0_IRQHandler(void) ALIAS(IntDefaultHandler);
void EINT1_IRQHandler(void) ALIAS(IntDefaultHandler);
void EINT2_IRQHandler(void) ALIAS(IntDefaultHandler);
void EINT3_IRQHandler(void) ALIAS(IntDefaultHandler);
void ADC_IRQHandler(void) ALIAS(IntDefaultHandler);
void BOD_IRQHandler(void) ALIAS(IntDefaultHandler);
void USB_IRQHandler(void) ALIAS(IntDefaultHandler);
void CAN_IRQHandler(void) ALIAS(IntDefaultHandler);
void DMA_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2S_IRQHandler(void) ALIAS(IntDefaultHandler);
void ETH_IRQHandler(void) ALIAS(IntDefaultHandler);
void RIT_IRQHandler(void) ALIAS(IntDefaultHandler);
void MCPWM_IRQHandler(void) ALIAS(IntDefaultHandler);
void QEI_IRQHandler(void) ALIAS(IntDefaultHandler);
void PLL1_IRQHandler(void) ALIAS(IntDefaultHandler);
void USBActivity_IRQHandler(void) ALIAS(IntDefaultHandler);
void CANActivity_IRQHandler(void) ALIAS(IntDefaultHandler);

extern void __main(void);
extern int main(void);
extern void _vStackTop(void);
WEAK extern void __valid_user_code_checksum();

extern void (* const g_pfnVectors[])(void);
__attribute__ ((used,section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    // Core Level - CM3
	// Hack for bootloader that insists on jumping to the vectors
	// as if they were code...
	// The problem here is compounded by the fact that FreeRTOS insists on
	// reading the stack pointer from here so that one has to stay. Just
	// make sure the sp translates to harmless code!

	&_vStackTop, // The initial stack pointer
	// ldr sp,[pc,24] ldr pc,[pc,24] solves this by moving those constants down a bit
//    (void*)0xd018f8df,
	(void*)0xf018f8df, // Only move ResetISR
//    ResetISR,                               // The reset handler

	NMI_Handler,                            // The NMI handler
    HardFault_Handler,                      // The hard fault handler
    MemManage_Handler,                      // The MPU fault handler
    BusFault_Handler,                       // The bus fault handler
    UsageFault_Handler,                     // The usage fault handler

	__valid_user_code_checksum,             // LPC MCU Checksum (ignored by bootloader)
//	  &_vStackTop, // The initial stack pointer (used by the ldp sp, [pc,24] above)
//    0,                                      // Reserved
    ResetISR,                               // The reset handler (used by the ldr pc,[pc,24] above)

	0,                                      // Reserved
    0,                                      // Reserved
	SVC_Handler,                            // SVCall handler
    DebugMon_Handler,                       // Debug monitor handler
    0,                                      // Reserved
	PendSV_Handler,                         // The PendSV handler
	SysTick_Handler,                        // The SysTick handler

    // Chip Level - LPC17
    WDT_IRQHandler,                         // 16, 0x40 - WDT
    TIMER0_IRQHandler,                      // 17, 0x44 - TIMER0
    TIMER1_IRQHandler,                      // 18, 0x48 - TIMER1
    TIMER2_IRQHandler,                      // 19, 0x4c - TIMER2
    TIMER3_IRQHandler,                      // 20, 0x50 - TIMER3
    UART0_IRQHandler,                       // 21, 0x54 - UART0
    UART1_IRQHandler,                       // 22, 0x58 - UART1
    UART2_IRQHandler,                       // 23, 0x5c - UART2
    UART3_IRQHandler,                       // 24, 0x60 - UART3
    PWM1_IRQHandler,                        // 25, 0x64 - PWM1
    I2C0_IRQHandler,                        // 26, 0x68 - I2C0
    I2C1_IRQHandler,                        // 27, 0x6c - I2C1
    I2C2_IRQHandler,                        // 28, 0x70 - I2C2
    SPI_IRQHandler,                         // 29, 0x74 - SPI
    SSP0_IRQHandler,                        // 30, 0x78 - SSP0
    SSP1_IRQHandler,                        // 31, 0x7c - SSP1
    PLL0_IRQHandler,                        // 32, 0x80 - PLL0 (Main PLL)
    RTC_IRQHandler,                         // 33, 0x84 - RTC
    EINT0_IRQHandler,                       // 34, 0x88 - EINT0
    EINT1_IRQHandler,                       // 35, 0x8c - EINT1
    EINT2_IRQHandler,                       // 36, 0x90 - EINT2
    EINT3_IRQHandler,                       // 37, 0x94 - EINT3
    ADC_IRQHandler,                         // 38, 0x98 - ADC
    BOD_IRQHandler,                         // 39, 0x9c - BOD
    USB_IRQHandler,                         // 40, 0xA0 - USB
    CAN_IRQHandler,                         // 41, 0xa4 - CAN
    DMA_IRQHandler,                         // 42, 0xa8 - GP DMA
    I2S_IRQHandler,                         // 43, 0xac - I2S
    ETH_IRQHandler,                         // 44, 0xb0 - Ethernet
    RIT_IRQHandler,                         // 45, 0xb4 - RITINT
    MCPWM_IRQHandler,                       // 46, 0xb8 - Motor Control PWM
    QEI_IRQHandler,                         // 47, 0xbc - Quadrature Encoder
    PLL1_IRQHandler,                        // 48, 0xc0 - PLL1 (USB PLL)
    USBActivity_IRQHandler,                 // 49, 0xc4 - USB Activity interrupt to wakeup
    CANActivity_IRQHandler,                 // 50, 0xc8 - CAN Activity interrupt to wakeup
};

//*****************************************************************************
// Functions to carry out the initialization of RW and BSS data sections. These
// are written as separate functions rather than being inlined within the
// ResetISR() function in order to cope with MCUs with multiple banks of
// memory.
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void data_init(unsigned int romstart, unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int *pulSrc = (unsigned int*) romstart;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = *pulSrc++;
}

__attribute__ ((section(".after_vectors")))
void bss_init(unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = 0;
}

//*****************************************************************************
// The following symbols are constructs generated by the linker, indicating
// the location of various points in the "Global Section Table". This table is
// created by the linker via the Code Red managed linker script mechanism. It
// contains the load address, execution address and length of each RW data
// section and the execution and length of each BSS (zero initialized) section.
//*****************************************************************************
extern unsigned int __data_section_table;
extern unsigned int __data_section_table_end;
extern unsigned int __bss_section_table;
extern unsigned int __bss_section_table_end;

//*****************************************************************************
// Reset entry point for your code.
// Sets up a simple runtime environment and initializes the C/C++
// library.
//*****************************************************************************
__attribute__ ((section(".after_vectors")))
void
ResetISR(void) {

    //
    // Copy the data sections from flash to SRAM.
    //
    unsigned int LoadAddr, ExeAddr, SectionLen;
    unsigned int *SectionTableAddr;

    // Load base address of Global Section Table
    SectionTableAddr = &__data_section_table;

    // Copy the data sections from flash to SRAM.
    while (SectionTableAddr < &__data_section_table_end) {
        LoadAddr = *SectionTableAddr++;
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        data_init(LoadAddr, ExeAddr, SectionLen);
    }
    // At this point, SectionTableAddr = &__bss_section_table;
    // Zero fill the bss segment
    while (SectionTableAddr < &__bss_section_table_end) {
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        bss_init(ExeAddr, SectionLen);
    }

    SystemInit();
#if defined (__REDLIB__)
    // Call the Redlib library, which in turn calls main()
    __main() ;
#else
    main();
#endif

    //
    // main() shouldn't return, but if it does, we'll attempt a restart
    //
    NVIC_SystemReset();
    while (1);
}

void IntDefaultHandler(void) {
	while(1);
}
