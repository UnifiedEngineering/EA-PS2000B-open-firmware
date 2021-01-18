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
#include <string.h>
#include "gpiomap.h"
#include "app_usbd_cfg.h"
#include "cdc_vcom.h"
#include "keypad.h"
#include "display.h"
#include "powersupply.h"
#include "stopwatch.h"

// NXP USB driver stuff
static USBD_HANDLE_T g_hUsb;
static uint8_t g_rxBuff[64];

extern const  USBD_HW_API_T hw_api;
extern const  USBD_CORE_API_T core_api;
extern const  USBD_CDC_API_T cdc_api;
// CDC class only
static const  USBD_API_T g_usbApi = {
	&hw_api,
	&core_api,
	0,
	0,
	0,
	&cdc_api,
	0,
	0x02221101,
};
const  USBD_API_T *g_pUsbApi = &g_usbApi;

void USB_IRQHandler(void)
{
	USBD_API->hw->ISR(g_hUsb);
}

// Find the address of interface descriptor for given class type.
USB_INTERFACE_DESCRIPTOR *find_IntfDesc(const uint8_t *pDesc, uint32_t intfClass)
{
	USB_COMMON_DESCRIPTOR *pD;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = 0;
	uint32_t next_desc_adr;

	pD = (USB_COMMON_DESCRIPTOR *) pDesc;
	next_desc_adr = (uint32_t) pDesc;

	while (pD->bLength) {
		/* is it interface descriptor */
		if (pD->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {

			pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) pD;
			/* did we find the right interface descriptor */
			if (pIntfDesc->bInterfaceClass == intfClass) {
				break;
			}
		}
		pIntfDesc = 0;
		next_desc_adr = (uint32_t) pD + pD->bLength;
		pD = (USB_COMMON_DESCRIPTOR *) next_desc_adr;
	}

	return pIntfDesc;
}

// num_decimals need to control where the decimal point goes
// If num_decimals == 0 then no decimal is shown,
// as num_decimal is increased DP moves into DP3/DP7, DP2/DP6 and finally DP1/DP5
static const disp_glyphs_t left_dp[4] = {GLYPH_MAX, GLYPH_DP3, GLYPH_DP2, GLYPH_DP1};
static const disp_glyphs_t right_dp[4] = {GLYPH_MAX, GLYPH_DP7, GLYPH_DP6, GLYPH_DP5};

static int32_t handle_encoder(conversions_t type, encoder_t encoder, uint32_t chnum, bool* increased, bool* changed) {
	int32_t tmp, delta, max;
	tmp = ps_get_setpoint(chnum, type);
	delta = get_encoder_delta(encoder);

	// Simple acceleration if turning knob fast
	if (delta > 1 || delta < -1) delta *= 10;
	if (delta > 10 || delta < -10) delta *= 2;

	// Support dynamic power limiting
	if (increased && delta > 0) *increased = true;

	// Support dynamic power limiting
	if (changed && delta) *changed = true;

	tmp += delta;

	max = ps_get_conv_info_ptr()[type].max_setpoint;
	if (tmp < 0 ) tmp = 0;
	if (tmp > max) tmp = max;

	return tmp;
}

typedef enum {
	DISP_READBACK = 0,
	DISP_SETPOINT,
	DISP_STATUS,
	DISP_EXTRA,
	DISP_MAX_VAL
} disp_t;

// Experimental front panel "UI".
static void testui_task( void* pvParameters ) {
	const conversion_info_t* scale = ps_get_conv_info_ptr();

    uint8_t dispbuf[11];
    char tmpbuf[9];

    vTaskDelay(1000);
	memset (dispbuf, 0, sizeof(dispbuf));
	disp_font_str(dispbuf, 0, 8, "x UEoS x");
	disp_update(DISP_CH1, dispbuf, 8, sizeof(dispbuf));
    vTaskDelay(1000);
    LED_TRACKING(false);

    uint32_t counter = 0;

    while(1) {
    	counter++;
    	// Only state 0 and 1 now
    	disp_t disp = (counter & 0x20) >> 5;
		static bool ch1onoff = false;
		static bool ch2onoff = false;
		static bool ch1lastonoff = false;
		static bool ch2lastonoff = false;
		static bool tracking = false;

		uint32_t rbch1volt,rbch1curr;
		uint32_t rbch2volt,rbch2curr;

		key_t newkeys = get_keys_pressed();
		if (newkeys & KEY_CH1_ON_OFF) ch1onoff = !ch1onoff;
		if (newkeys & KEY_CH2_ON_OFF) ch2onoff = !ch2onoff;
//		if (newkeys & KEY_TRACKING) tracking = !tracking;
//		LED_TRACKING(tracking);

		uint32_t maxpower = scale[CONVERSION_POWER].max_setpoint;
		bool voltincreased = false;
		bool changed = false;
		int32_t newvolt1 = handle_encoder(CONVERSION_VOLTAGE, ENC_CH1_VOLT, 0, &voltincreased, &changed);
		int32_t newcurr1 = handle_encoder(CONVERSION_CURRENT, ENC_CH1_CURR, 0, NULL, &changed);
		uint32_t newpower1 = newvolt1 * newcurr1;
		if (newpower1 > maxpower) {
			if (voltincreased) { // Reduce current as the user wanted to increase voltage
				newcurr1 = maxpower / newvolt1;
			} else { // Reduce voltage as user wanted to increase current
				newvolt1 = maxpower / newcurr1;
			}
		}

		changed = voltincreased = false;
		int32_t newvolt2 = handle_encoder(CONVERSION_VOLTAGE, ENC_CH2_VOLT, 1, &voltincreased, &changed);
		int32_t newcurr2 = handle_encoder(CONVERSION_CURRENT, ENC_CH2_CURR, 1, NULL, &changed);
		uint32_t newpower2 = newvolt2 * newcurr2;
		if (newpower2 > maxpower) {
			if (voltincreased) { // Reduce current as the user wanted to increase voltage
				newcurr2 = maxpower / newvolt2;
			} else { // Reduce voltage as user wanted to increase current
				newvolt2 = maxpower / newcurr2;
			}
		}

		ps_set_setpoints(0, newvolt1, newcurr1, ch1onoff);
		ps_set_setpoints(1, newvolt2, newcurr2, ch2onoff);

		vTaskDelay(50);

		rbch1volt = ps_get_readback(0, CONVERSION_VOLTAGE);
		rbch2volt = ps_get_readback(1, CONVERSION_VOLTAGE);
		rbch1curr = ps_get_readback(0, CONVERSION_CURRENT);
		rbch2curr = ps_get_readback(1, CONVERSION_CURRENT);
		uint32_t ch1status = ps_get_status(0);
		uint32_t ch2status = ps_get_status(1);
		// Major hack here as the first response after an on/off toggle we'll get
		// from the original module firmware will always be old on/off information
		if (ch1onoff == ch1lastonoff) {
			ch1onoff = ch1status & 1;
		} else {
			ch1lastonoff = ch1onoff;
		}
		if (ch2onoff == ch2lastonoff) {
			ch2onoff = ch2status & 1;
		} else {
			ch2lastonoff = ch2onoff;
		}

		uint32_t leftval, rightval, leftmindigits, rightmindigits;
		switch(disp) {
		case DISP_READBACK:
		case DISP_EXTRA:
			leftval = rbch1volt;
			leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
			rightval = rbch1curr;
			rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
			break;
		case DISP_SETPOINT:
			leftval = newvolt1;
			leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
			rightval = newcurr1;
			rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
			break;
		case DISP_STATUS:
			leftval = ch1status >> 8;
			rightval = ch1status & 0xff;
			leftmindigits = rightmindigits = 1;
		}
		memset (dispbuf, 0, sizeof(dispbuf));
		memset (tmpbuf, 0, sizeof(tmpbuf));
		uint32_t numch = disp_format_digits(NULL, leftval, leftmindigits);
		numch = disp_format_digits(&tmpbuf[4 - numch], leftval, leftmindigits);
		numch = disp_format_digits(NULL, rightval, rightmindigits);
		numch = disp_format_digits(&tmpbuf[8 - numch], rightval, rightmindigits);
		numch = 8;
		disp_font_str(dispbuf, 0, numch, ch1status == 0xffffffff ? "--------" : tmpbuf);
		switch(disp) {
		case DISP_SETPOINT:
			disp_add_glyph(dispbuf, GLYPH_PRESET);
			// Fallback intentional
		case DISP_READBACK:
		case DISP_EXTRA:
			disp_add_glyph(dispbuf, left_dp[ scale[CONVERSION_VOLTAGE].num_decimals ]);
			disp_add_glyph(dispbuf, GLYPH_VOLTAGE);
			disp_add_glyph(dispbuf, right_dp[ scale[CONVERSION_CURRENT].num_decimals ]);
			disp_add_glyph(dispbuf, GLYPH_CURRENT);
			break;
		case DISP_STATUS:
			;
		}
		disp_add_glyph(dispbuf, GLYPH_DIVIDER_LINE);
		if (disp == DISP_READBACK || disp == DISP_EXTRA) {
			if (ch1status & 1) disp_add_glyph(dispbuf, ch1status & 0x06 ? GLYPH_CC : GLYPH_CV);
		}
		disp_add_glyph(dispbuf, ch1status & 1 ? GLYPH_ON : GLYPH_OFF);
		disp_add_glyph(dispbuf, ch1status & 0x80 ? GLYPH_OT : GLYPH_MAX);
		disp_update(DISP_CH1, dispbuf, 8, sizeof(dispbuf));

		switch(disp) {
		case DISP_READBACK:
		case DISP_EXTRA:
			leftval = rbch2volt;
			leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
			rightval = rbch2curr;
			rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
			break;
		case DISP_SETPOINT:
			leftval = newvolt2;
			leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
			rightval = newcurr2;
			rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
			break;
		case DISP_STATUS:
			leftval = ch2status >> 8;
			rightval = ch2status & 0xff;
			leftmindigits = rightmindigits = 1;
		}
		memset (dispbuf, 0, sizeof(dispbuf));
		memset (tmpbuf, 0, sizeof(tmpbuf));
		numch = disp_format_digits(NULL, leftval, leftmindigits);
		numch = disp_format_digits(&tmpbuf[4 - numch], leftval, leftmindigits);
		numch = disp_format_digits(NULL, rightval, rightmindigits);
		numch = disp_format_digits(&tmpbuf[8 - numch], rightval, rightmindigits);
		numch = 8;
		disp_font_str(dispbuf, 0, numch, ch2status == 0xffffffff ? "--------" : tmpbuf);
		switch(disp) {
		case DISP_SETPOINT:
			disp_add_glyph(dispbuf, GLYPH_PRESET);
			// Fallback intentional
		case DISP_READBACK:
		case DISP_EXTRA:
			disp_add_glyph(dispbuf, left_dp[ scale[CONVERSION_VOLTAGE].num_decimals ]);
			disp_add_glyph(dispbuf, GLYPH_VOLTAGE);
			disp_add_glyph(dispbuf, right_dp[ scale[CONVERSION_CURRENT].num_decimals ]);
			disp_add_glyph(dispbuf, GLYPH_CURRENT);
			break;
		case DISP_STATUS:
			;
		}
		disp_add_glyph(dispbuf, GLYPH_DIVIDER_LINE);
		if (disp == DISP_READBACK || disp == DISP_EXTRA) {
			if (ch2status & 1) disp_add_glyph(dispbuf, ch2status & 0x06 ? GLYPH_CC : GLYPH_CV);
		}
		disp_add_glyph(dispbuf, ch2status & 1 ? GLYPH_ON : GLYPH_OFF);
		disp_add_glyph(dispbuf, ch2status & 0x80 ? GLYPH_OT : GLYPH_MAX);
		disp_update(DISP_CH2, dispbuf, 8, sizeof(dispbuf));

    }

}

// NXP example code for CDC ACM VCOM, to be expanded, right now just echoes input
static void usb_task( void* pvParameters ) {
	USBD_API_INIT_PARAM_T usb_param;
	USB_CORE_DESCS_T desc;
	ErrorCode_t ret = LPC_OK;
	uint32_t prompt = 0, rdCnt = 0;

	/* enable USB PLL and clocks */
	Chip_USB_Init();
	LPC_USB->USBClkCtrl = 0x12;                /* Dev, AHB clock enable */
	while ((LPC_USB->USBClkSt & 0x12) != 0x12);

	/* initialize call back structures */
	memset((void *) &usb_param, 0, sizeof(USBD_API_INIT_PARAM_T));
	usb_param.usb_reg_base = LPC_USB_BASE + 0x200;
	usb_param.max_num_ep = 3;
	usb_param.mem_base = USB_STACK_MEM_BASE;
	usb_param.mem_size = USB_STACK_MEM_SIZE;

	/* Set the USB descriptors */
	desc.device_desc = (uint8_t *) &USB_DeviceDescriptor[0];
	desc.string_desc = (uint8_t *) &USB_StringDescriptor[0];
	/* Note, to pass USBCV test full-speed only devices should have both
	   descriptor arrays point to same location and device_qualifier set to 0.
	 */
	desc.high_speed_desc = (uint8_t *) &USB_FsConfigDescriptor[0];
	desc.full_speed_desc = (uint8_t *) &USB_FsConfigDescriptor[0];
	desc.device_qualifier = 0;

	/* USB Initialization */
	ret = USBD_API->hw->Init(&g_hUsb, &desc, &usb_param);
	if (ret == LPC_OK) {

		/* Init VCOM interface */
		ret = vcom_init(g_hUsb, &desc, &usb_param);
		if (ret == LPC_OK) {
			/*  enable USB interrupts */
			NVIC_SetPriority(USB_IRQn, 2);
			NVIC_EnableIRQ(USB_IRQn);
			/* now connect */
			USBD_API->hw->Connect(g_hUsb, 1);
		}
	}

	while (1) {
		/* Check if host has connected and opened the VCOM port */
		if ((vcom_connected() != 0) && (prompt == 0)) {
			vcom_write("Hello World!!\r\n", 15);
			prompt = 1;
		}
		/* If VCOM port is opened echo whatever we receive back to host. */
		if (prompt) {
			rdCnt = vcom_bread(&g_rxBuff[0], 64);
			if (rdCnt) {
				vcom_write(&g_rxBuff[0], rdCnt);
			}
		}
		/* Sleep until next IRQ happens */
//		__WFI();
		vTaskDelay(1);
	}

}

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

    xTaskCreate( testui_task,            /* The function that implements the task. */
                "testui",                            /* The text name assigned to the task - for debug only as it is not used by the kernel. */
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
