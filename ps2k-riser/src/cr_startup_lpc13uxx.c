#include "chip.h"

#define WEAK __attribute__ ((weak))
#define ALIAS(f) __attribute__ ((weak, alias (#f)))

extern void SystemInit(void);

void ResetISR(void) __attribute__ ((naked));
WEAK void IntDefaultHandler(void);
void NMI_Handler(void) ALIAS(IntDefaultHandler);
void HardFault_Handler(void) ALIAS(IntDefaultHandler);
void MemManage_Handler(void) ALIAS(IntDefaultHandler);
void BusFault_Handler(void) ALIAS(IntDefaultHandler);
void UsageFault_Handler(void) ALIAS(IntDefaultHandler);
void SVC_Handler(void) ALIAS(IntDefaultHandler);
void DebugMon_Handler(void) ALIAS(IntDefaultHandler);
void PendSV_Handler(void) ALIAS(IntDefaultHandler);
void SysTick_Handler(void) ALIAS(IntDefaultHandler);
void PIN_INT0_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT1_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT2_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT3_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT4_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT5_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT6_IRQHandler(void) ALIAS(IntDefaultHandler);
void PIN_INT7_IRQHandler(void) ALIAS(IntDefaultHandler);
void GINT0_IRQHandler(void) ALIAS(IntDefaultHandler);
void GINT1_IRQHandler(void) ALIAS(IntDefaultHandler);
void SSP1_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2C_IRQHandler(void) ALIAS(IntDefaultHandler);
void SSP0_IRQHandler(void) ALIAS(IntDefaultHandler);
void USB_IRQHandler(void) ALIAS(IntDefaultHandler);
void USB_FIQHandler(void) ALIAS(IntDefaultHandler);
void ADC_IRQHandler(void) ALIAS(IntDefaultHandler);
void WDT_IRQHandler(void) ALIAS(IntDefaultHandler);
void BOD_IRQHandler(void) ALIAS(IntDefaultHandler);
void FMC_IRQHandler(void) ALIAS(IntDefaultHandler);
void OSCFAIL_IRQHandler(void) ALIAS(IntDefaultHandler);
void PVTCIRCUIT_IRQHandler(void) ALIAS(IntDefaultHandler);
void USBWakeup_IRQHandler(void) ALIAS(IntDefaultHandler);
void RIT_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER16_0_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER16_1_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER32_0_IRQHandler (void) ALIAS(IntDefaultHandler);
void TIMER32_1_IRQHandler (void) ALIAS(IntDefaultHandler);
void UART_IRQHandler (void) ALIAS(IntDefaultHandler);

#if defined (__REDLIB__)
extern void __main(void);
#else
extern int main(void);
#endif

extern void _vStackTop(void);
WEAK extern void __valid_user_code_checksum();

extern void (* const g_pfnVectors[])(void);
__attribute__ ((used,section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    // Core Level - CM3
        &_vStackTop,            //  initial stack pointer
        ResetISR,               // The reset handler
        NMI_Handler,            // The NMI handler
        HardFault_Handler,      // The hard fault handler
        MemManage_Handler,      // The MPU fault handler
        BusFault_Handler,       // The bus fault handler
        UsageFault_Handler,     // The usage fault handler
		__valid_user_code_checksum, // LPC MCU Checksum
        0,                      // Reserved
        0,                      // Reserved
        0,                      // Reserved
        SVC_Handler,            // SVCall handler
        DebugMon_Handler,       // Debug monitor handler
        0,                      // Reserved
        PendSV_Handler,         // The PendSV handler
        SysTick_Handler,        // The SysTick handler

        // LPC13U External Interrupts
        PIN_INT0_IRQHandler,    // All GPIO pin can be routed ...
        PIN_INT1_IRQHandler,    // ... to PIN_INTx
        PIN_INT2_IRQHandler,
        PIN_INT3_IRQHandler,
        PIN_INT4_IRQHandler,
        PIN_INT5_IRQHandler,
        PIN_INT6_IRQHandler,
        PIN_INT7_IRQHandler,
        GINT0_IRQHandler,
        GINT1_IRQHandler,       // PIO0 (0:7)
        0,
        0,
        RIT_IRQHandler,
        0,
        SSP1_IRQHandler,        // SSP1
        I2C_IRQHandler,         //  I2C
        TIMER16_0_IRQHandler,   // 16-bit Counter-Timer 0
	    TIMER16_1_IRQHandler,   // 16-bit Counter-Timer 1
	    TIMER32_0_IRQHandler,   // 32-bit Counter-Timer 0
	    TIMER32_1_IRQHandler,   // 32-bit Counter-Timer 1
        SSP0_IRQHandler,        // SSP0
        UART_IRQHandler,        // UART
        USB_IRQHandler,         // USB IRQ
        USB_FIQHandler,         // USB FIQ
        ADC_IRQHandler,         // A/D Converter
        WDT_IRQHandler,         // Watchdog timer
        BOD_IRQHandler,         // Brown Out Detect
//        FMC_IRQHandler,         // IP2111 Flash Memory Controller
//        OSCFAIL_IRQHandler,     // OSC FAIL
//        PVTCIRCUIT_IRQHandler,  // PVT CIRCUIT
//        USBWakeup_IRQHandler,   // USB wake up
//        0,
    };

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
__attribute__ ((aligned (2)))
void
ResetISR(void) {

	// When RAM-loading VTOR isn't updated properly
	SCB->VTOR = (uint32_t)g_pfnVectors;
	// Neither is MSP, lets fix that
	__set_MSP((uint32_t)&_vStackTop);

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

    // Optionally enable Cortex-M3 SWV trace (off by default at reset)
    // Note - your board support must also set up pinmuxing such that
    // SWO is output on GPIO PIO0-9 (FUNC3)
#if !defined (DONT_ENABLE_SWVTRACECLK)
	// Write 0x1 to TRACECLKDIV – Trace divider
	volatile unsigned int *TRACECLKDIV = (unsigned int *) 0x400480AC;
	*TRACECLKDIV = 1;
#endif

#if defined (__USE_CMSIS) || defined (__USE_LPCOPEN)
    SystemInit();
#endif

#if defined (__REDLIB__)
    // Call the Redlib library, which in turn calls main()
    __main() ;
#else
    main();
#endif
    //
    // main() shouldn't return, but if it does, we'll just reboot
    //
    NVIC_SystemReset();
    while (1);
}

//*****************************************************************************
//
// Processor ends up here if an unexpected interrupt occurs or a handler
// is not present in the application code.
//
//*****************************************************************************
// Don't place anything else than ResetISR in after_vectors
// as the boot ROM requires ResetISR to be aligned to dword (even though the MCU isn't requiring that)
//__attribute__ ((section(".after_vectors")))
void IntDefaultHandler(void) {
    //
    // Go into an infinite loop.
    //
    while (1) {
    }
}
