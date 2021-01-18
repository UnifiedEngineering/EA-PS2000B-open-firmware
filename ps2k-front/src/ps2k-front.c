/*
 * ps2k-front.c - Alternate PS2300-series front panel firmware
 *
 * Copyright (C) 2021 Werner Johansson, wj@unifiedengineering.se
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <FreeRTOS.h>
#include <task.h>
#include "gpiomap.h"
#include "keypad.h"
#include "display.h"
#include "powersupply.h"
#include "usb.h"
#include "ui.h"
#include "stopwatch.h"

void main( void ) __attribute__( ( noreturn ) );
void main(void) {

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    LPC_GPIO[0].DIR = PORT0_DIR;
    LPC_GPIO[1].DIR = PORT1_DIR;
    LPC_GPIO[2].DIR = PORT2_DIR;
    LPC_GPIO[4].DIR = PORT4_DIR;

    // Turn tracking LED on
    LED_TRACKING(true);

    StopWatch_Init();
	keypad_init();
    disp_init(DISP_BOTH);
	ps_init();

    xTaskCreate( ui_task,            /* The function that implements the task. */
                "ui",                            /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                configMINIMAL_STACK_SIZE,        /* The size of the stack to allocate to the task. */
                NULL,                            /* The parameter passed to the task - not used in this case. */
				configMAX_PRIORITIES - 2, /* The priority assigned to the task. */
                NULL );                          /* The task handle is not required, so NULL is passed. */

	xTaskCreate( usb_task,            /* The function that implements the task. */
                "usb",                            /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                configMINIMAL_STACK_SIZE + 256,        /* The size of the stack to allocate to the task. */
                NULL,                            /* The parameter passed to the task - not used in this case. */
				configMAX_PRIORITIES - 3, /* The priority assigned to the task. */
                NULL );                          /* The task handle is not required, so NULL is passed. */

    vTaskStartScheduler();

    // If we end up here something broke FreeRTOS
    NVIC_SystemReset();
    while(1);
}

void vAssertCalled( uint32_t line, char* filename ) {
	disp_debug(filename, line);
	while(1);
}

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress ) __attribute__((externally_visible));
void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress ) {
	uint32_t r0 = pulFaultStackAddress[ 0 ];
	uint32_t r1 = pulFaultStackAddress[ 1 ];
	uint32_t r2 = pulFaultStackAddress[ 2 ];
	uint32_t r3 = pulFaultStackAddress[ 3 ];

	uint32_t r12 = pulFaultStackAddress[ 4 ];
	uint32_t lr = pulFaultStackAddress[ 5 ];
	uint32_t pc = pulFaultStackAddress[ 6 ];
	uint32_t psr = pulFaultStackAddress[ 7 ];

	uint32_t cfsr = SCB->CFSR;

	while(1) {
		LPC_GPIO[1].SET = 1UL << 29;
		disp_debug("hf  cfsr", cfsr);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf fu pc", pc);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf   psr", psr);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf    r0", r0);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf    r1", r1);
        StopWatch_DelayUs(0x100000);
		LPC_GPIO[1].CLR = 1UL << 29;
		disp_debug("hf    r2", r2);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf    r3", r3);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf   r12", r12);
        StopWatch_DelayUs(0x100000);
		disp_debug("hf    lr", lr);
        StopWatch_DelayUs(0x100000);
	}
}

void HardFault_Handler( void ) __attribute__( ( naked ) );
void HardFault_Handler(void) {
	__asm volatile
	    (
	        " tst lr, #4                                                \n"
	        " ite eq                                                    \n"
	        " mrseq r0, msp                                             \n"
	        " mrsne r0, psp                                             \n"
	        " ldr r1, [r0, #24]                                         \n"
	        " ldr r2, handler2_address_const                            \n"
	        " bx r2                                                     \n"
	        " handler2_address_const: .word prvGetRegistersFromStack    \n"
	    );
}

int __sys_istty(int unk) {
	return 0;
}

int __sys_flen(int unk) {
	return 0;
}

int __sys_seek(int unk, int unk2) {
	return 0;
}

void __sys_appexit(void) {
}
