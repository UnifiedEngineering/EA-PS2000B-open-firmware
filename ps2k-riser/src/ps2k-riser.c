/*
 * ps2k-riser.c - Alternate PS2000_LT riser board firmware
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

// ISP commands use on-chip RAM from 0x1000017C to 0x1000025B and 0x10001f00 to 0x10001fff
// according to the LPC1315 user manual. This code will be RAM-loaded at 0x10000300 (up to
// 0x10001fe0) and the stack top will be set to 0x10001fe0. (top 32 bytes reserved for IAP)
// Let's see if the boot ROM stack will be an issue
// (the boot ROM initializes the sp to 0x10000ffc, and then seems to update it.
// sp was 0x10001f70 when this code reached main ran before msp (and vtor) now being
// set during startup

#include "chip.h"
#include "debug.h"
#include "ee.h"
#include <string.h>

static LPC_TIMER_T* timers[] = { LPC_TIMER16_0, LPC_TIMER16_1, LPC_TIMER32_1 };
#define NUM_TIMERS (sizeof(timers) / sizeof(timers[0]))

#define _BV(x) (1 << x)
#define STATUS_OVERTEMP _BV(7)
#define STATUS_MODE_MASK (_BV(2) | _BV(1))
#define STATUS_CC _BV(2)
#define STATUS_OUTPUT_ON _BV(0)

static void output_enable(bool on) {
	LPC_GPIO_PORT->B[0][17] = !on;
}

// RAM copies of EEPROM data
static ee_id s_id;
static ee_cal s_cal;
static ee_setpoint s_setpoint;

bool s_setpoint_updated = false;
uint16_t readback_volt = 0;
uint16_t readback_curr = 0;

typedef enum {
	AD_REF = 0,
	AD_CURR,
	AD_2,
	AD_3,
	AD_4,
	AD_VOLT,
	AD_LOOP,
	AD_TEMP,
	AD_MAX_VAL
} adch_t;

static uint16_t s_adc_result[AD_MAX_VAL];

static const uint8_t s_adscan[] = {AD_REF, AD_CURR, AD_VOLT, AD_LOOP, AD_TEMP};
#define AD_SCAN_LEN (sizeof(s_adscan) / sizeof(s_adscan[0]))

// ADC input below ~1.428V triggers OT with oem firmware, this is the scaled value I
// measured at the same input.
#define OVERTEMP_THRES (14928)
static uint16_t s_adc_temp_compare = 0;
static bool s_overtemp = false;

static uint32_t calc_checksum(uint8_t* buf, uint32_t size) {
	uint32_t result = 0;
	for (int i = 0; i < size; i++) {
		result += buf[i];
	}
	return result;
}

static uint32_t s_numrx = 0;
static uint8_t s_rxbuf[16];
#define RX_SIZE (sizeof(s_rxbuf))

#define PWMSHIFT (16)
static void update_setpoint(cal_t id) {
	uint32_t tmp = s_cal.cal[id].gain;

	switch(id) {
	case CAL_VOLT_SET:
		tmp *= s_setpoint.voltage;
		break;
	case CAL_CURR_SET:
		tmp *= s_setpoint.current;
		break;
	case CAL_VOLT_READ:
	case CAL_CURR_READ:
	case CAL_REF_READ:
	case CAL_TEMP_READ:
	case CAL_MAX_VAL:
		; // Not applicable here
	}

	tmp += 1 << (PWMSHIFT - 1); // Round
	tmp >>= PWMSHIFT;
	tmp += s_cal.cal[id].offset;

	switch(id) {
	case CAL_VOLT_SET:
//		Chip_TIMER_SetMatch(LPC_TIMER32_1, 1, 2048-(tmp>>3)); // Inverted PWM duty for MAT1 (voltage)
		Chip_TIMER_SetMatch(LPC_TIMER32_1, 1, 16383-(tmp)); // Inverted PWM duty for MAT1 (voltage)
		break;
	case CAL_CURR_SET:
//		Chip_TIMER_SetMatch(LPC_TIMER16_1, 0, 2048-(tmp>>3)); // Inverted PWM duty for MAT0 (current)
		Chip_TIMER_SetMatch(LPC_TIMER16_1, 0, 16383-(tmp)); // Inverted PWM duty for MAT0 (current)
		break;
	case CAL_VOLT_READ:
	case CAL_CURR_READ:
	case CAL_REF_READ:
	case CAL_TEMP_READ:
	case CAL_MAX_VAL:
		; // Not applicable here
	}
}

#define ADCSHIFT (17)
static uint32_t convert_adc_readback(cal_t id, uint32_t value) {
	uint32_t tmp = s_cal.cal[id].gain;
	tmp *= value;

	// With a (oversampled) 16-bit ADC value the result has to be shifted 17 bits
	tmp += 1 << (ADCSHIFT - 1); // Round
	tmp >>= ADCSHIFT;
	tmp += s_cal.cal[id].offset;
	if (tmp & 0x80000000) tmp = 0; // Clamp negative numbers
	return tmp;
}

static void handle_set_setpoint(uint32_t len) {

	uint8_t newonoff = s_rxbuf[1];
	uint16_t newvolt = s_rxbuf[2] << 8 | s_rxbuf[3];
	uint16_t newcurr = s_rxbuf[4] << 8 | s_rxbuf[5];

	if (s_overtemp) newonoff = 0;
	// Sanity check against max power
	uint32_t power = (newvolt * newcurr) >> 16;
	if (power < s_id.max_out_power) {
		if (newvolt != s_setpoint.voltage) {
			s_setpoint.voltage = newvolt;
			s_setpoint_updated = true;
			update_setpoint(CAL_VOLT_SET);
		}
		if (newcurr != s_setpoint.current) {
			s_setpoint.current = newcurr;
			s_setpoint_updated = true;
			update_setpoint(CAL_CURR_SET);
		}
		output_enable (newonoff & 1);
	} else {
		ITM_SendChar('R');
	}

	readback_volt = convert_adc_readback(CAL_VOLT_READ, s_adc_result[AD_VOLT]);
	readback_curr = convert_adc_readback(CAL_CURR_READ, s_adc_result[AD_CURR]);

	bool cc = false;
	if (!s_overtemp && newonoff) {
		// Figure out how much lower the actual readback is from setpoint
		// If more than 0.25% lower (64 steps) we're in CC mode
		int32_t tmp = s_setpoint.voltage;
		tmp -= readback_volt;
		if (tmp >= 64) cc = true;
	}

	uint8_t resp[8];
	resp[0] = 0x17;
	resp[1] = (s_overtemp ? STATUS_OVERTEMP : 0) |
			(cc ? STATUS_CC : 0) |
			(newonoff ? STATUS_OUTPUT_ON : 0); // ps on/off, cc operation and overtemp
	resp[2] = readback_volt >> 8;
	resp[3] = readback_volt & 0xff;
	resp[4] = readback_curr >> 8;
	resp[5] = readback_curr & 0xff;
	resp[6] = 0x00; // ?
	resp[7] = (uint8_t)calc_checksum(resp, 7);
	for (uint32_t i = 0; i < 8; i++) {
		LPC_USART->THR = resp[i];
	}
}

static void handle_set_get_obj(uint32_t len) {
	uint8_t resp[16];
	uint32_t rlen = 1; // Skip first byte, will be updated when we have the length
	uint8_t obj = s_rxbuf[1];
	if (len == 2) { // Get
		resp[rlen++] = obj;
		switch (obj) {
		case 3:
			resp[rlen++] = s_setpoint.ovp >> 8;
			resp[rlen++] = s_setpoint.ovp & 0xff;
			break;
		case 4:
			resp[rlen++] = s_setpoint.ocp >> 8;
			resp[rlen++] = s_setpoint.ocp & 0xff;
			break;
		case 9:
			resp[rlen++] = s_setpoint.voltage >> 8;
			resp[rlen++] = s_setpoint.voltage & 0xff;
			break;
		case 0xa:
			resp[rlen++] = s_setpoint.current >> 8;
			resp[rlen++] = s_setpoint.current & 0xff;
			break;
		}
	} else { // Set
		// Not implemented yet
	}
	resp[0] = 0x80 | rlen;
	resp[rlen] = (uint8_t)calc_checksum(resp, rlen);
	rlen++;
	for (uint32_t i = 0; i < rlen; i++) {
		LPC_USART->THR = resp[i];
	}
}

static void parse_rxbuf(void) {
	// Parse request
	uint32_t len = s_rxbuf[0] & 0xf;
	switch (s_rxbuf[0] & 0xf0) {
	case 0x10:
		handle_set_setpoint(len);
		break;
	case 0x80:
		handle_set_get_obj(len);
		break;
	}
}

void UART_IRQHandler(void) {
	uint32_t iir = LPC_USART->IIR;

	while(LPC_USART->LSR & UART_LSR_RDR) {
		uint32_t tmp = LPC_USART->RBR; // Read rx data
		if (s_numrx < (RX_SIZE - 1)) {
			s_rxbuf[s_numrx++] = (uint8_t)tmp;
		}
	}

	// Minimum packet type+len byte, payload byte and checksum byte
	if (s_numrx > 2 && (s_numrx - 1) == (s_rxbuf[0] & 0xf) &&
			s_rxbuf[s_numrx - 1] == (uint8_t)calc_checksum(s_rxbuf, s_numrx - 1)) {
		ITM_SendChar('G');
		parse_rxbuf();
		s_numrx = 0;
	}
	if (s_numrx && (iir & UART_IIR_INTID_MASK) == UART_IIR_INTID_CTI) {
		// timeout, restart next transmission from the beginning no matter what
		ITM_SendChar('0' + s_numrx);
		s_numrx = 0;
	}
}

static uint32_t s_adc_cr = 0;
static uint32_t s_adc_state = 0;
static uint32_t s_adc_accumulator = 0;
static uint8_t s_scanstate = 0;

static void adc_setup_burst(uint32_t ch) {
	LPC_ADC->INTEN = 0;
	s_adc_cr &= ~(0xff | ADC_CR_BURST);
	LPC_ADC->CR = s_adc_cr;
	s_adc_accumulator = 0;
	s_adc_state = ch << 16;
	s_adc_cr |= ADC_CR_CH_SEL(ch);
	LPC_ADC->CR = s_adc_cr;
	// Activate the interrupt corresponding to the selected channel
	LPC_ADC->INTEN = ADC_CR_CH_SEL(ch);
	s_adc_cr |= ADC_CR_BURST;
	LPC_ADC->CR = s_adc_cr;
}

void ADC_IRQHandler(void) {
//	ITM_SendChar('A');
	uint32_t reqch = (s_adc_state >> 16) & 0x7;
	uint32_t data = LPC_ADC->DR[reqch];

	if (!ADC_DR_DONE(data)) {
		// Ignore
//		ITM_SendChar('I');
	} else {
		s_adc_accumulator += ADC_DR_RESULT(data);
		s_adc_state++;
		// 8192 samples per channel at 14.4MHz ADC clock results in
		// ~11 measurements per second per channel with 5 active channels
		if ((s_adc_state & 0x1fff) == 0) { // 8192 samples?
			uint32_t tmp = s_adc_accumulator;
			s_scanstate++;
			if (s_scanstate >= AD_SCAN_LEN) s_scanstate = 0;
			adc_setup_burst(s_adscan[s_scanstate]);
			tmp += 1 << 8; // Round
			tmp >>= 9; // 4 bits oversampling with 8192 samples requires accumulator to be /512
			s_adc_result[reqch] = tmp;
			if (reqch == AD_TEMP) {
//				printhex_itm("ad:  ", reqch << 28 | tmp);
//				printhex_itm("comp:", s_adc_temp_compare);
				if (tmp < s_adc_temp_compare) {
					s_overtemp = true;
					output_enable(false);
				} else {
					s_overtemp = false;
				}
			}
		}
	}
}

int main(void) {
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    //Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_GPIO);
	//Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_IOCON);

	// All unused GPIOs are pulled up by default so don't touch them

	// Set pio0.17 as output (output disable)
	LPC_GPIO_PORT->DIR[0] = 1 << 17;
	output_enable(false);

	// ADC pinmux
	LPC_IOCON->PIO0[11] = IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_ADMODE_EN; // ad0 reference
	LPC_IOCON->PIO0[12] = IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_ADMODE_EN; // ad1 current
	LPC_IOCON->PIO0[16] = IOCON_FUNC1 | IOCON_MODE_INACT | IOCON_ADMODE_EN; // ad5 voltage
	LPC_IOCON->PIO0[22] = IOCON_FUNC1 | IOCON_MODE_INACT | IOCON_ADMODE_EN; // ad6 loopback
	LPC_IOCON->PIO0[23] = IOCON_FUNC1 | IOCON_MODE_INACT | IOCON_ADMODE_EN; // ad7 temperature

	// UART pinmux
	Chip_UART_Init(LPC_USART);
	Chip_UART_TXEnable(LPC_USART);
	LPC_IOCON->PIO0[18] = IOCON_FUNC1 | IOCON_MODE_INACT | IOCON_RESERVED_BIT_7; // uart rxd
	LPC_IOCON->PIO0[19] = IOCON_FUNC1 | IOCON_MODE_PULLUP | IOCON_RESERVED_BIT_7; // uart txd

	// SWO
	LPC_IOCON->PIO0[9] = IOCON_FUNC3 | IOCON_MODE_PULLUP | IOCON_RESERVED_BIT_7; // SWO output

	init_swo();

	// PWM outputs
	LPC_IOCON->PIO0[8] = IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_RESERVED_BIT_7; // CT16B0_MAT0 loops to ad ch6?
	LPC_IOCON->PIO0[13] = IOCON_FUNC3 | IOCON_MODE_INACT | IOCON_DIGMODE_EN; // CT32B1_MAT0 unknown (pin 1 of X3 to power supply, unused)
	LPC_IOCON->PIO0[14] = IOCON_FUNC3 | IOCON_MODE_INACT | IOCON_DIGMODE_EN; // CT32B1_MAT1 voltage
	LPC_IOCON->PIO0[21] = IOCON_FUNC1 | IOCON_MODE_INACT | IOCON_RESERVED_BIT_7; // CT16B1_MAT0 current

	for (uint32_t i = 0; i < NUM_TIMERS; i++) {
		Chip_TIMER_Init(timers[i]);
		Chip_TIMER_Reset(timers[i]);
		Chip_TIMER_SetMatch(timers[i], 0, 16384 /*2048*/); // Inverted PWM duty for MAT0
		Chip_TIMER_SetMatch(timers[i], 1, 16384 /*2048*/); // Inverted PWM duty for MAT1
		Chip_TIMER_SetMatch(timers[i], 3, 16384 /*2048*/); // PWM period using MAT3
		Chip_TIMER_ResetOnMatchEnable(timers[i], 3);
		timers[i]->PWMC = (1 << 1) | (1 << 0); // MAT0 & 1
		Chip_TIMER_Enable(timers[i]);
	}

	iap_ee_read(EE_ID_START, &s_id, sizeof(s_id));
	iap_ee_read(EE_CAL_START, &s_cal, sizeof(s_cal));
	iap_ee_read(EE_SETPOINT_START, &s_setpoint, sizeof(s_setpoint));

	Chip_TIMER_SetMatch(LPC_TIMER16_0, 0, 1536); // Inverted PWM duty for MAT0 (loop)
	//Chip_TIMER_SetMatch(LPC_TIMER16_1, 0, 1024); // Inverted PWM duty for MAT0 (current)
	//Chip_TIMER_SetMatch(LPC_TIMER32_1, 0, 2048); // Inverted PWM duty for MAT0 (X3 pin 1, off)
	//Chip_TIMER_SetMatch(LPC_TIMER32_1, 1, 2040); // Inverted PWM duty for MAT1 (voltage)

	update_setpoint(CAL_VOLT_SET);
	update_setpoint(CAL_CURR_SET);

	// Front panel UART communications at 500kbps
	Chip_UART_SetBaud(LPC_USART, 500000);
	Chip_UART_ConfigData(LPC_USART, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT));
	Chip_UART_SetupFIFOS(LPC_USART, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV3));

	// Pre-calculate over-temperature threshold (ad reading below this triggers alarm and output disable)
	s_adc_temp_compare = ((1 << ADCSHIFT) * (OVERTEMP_THRES - s_cal.cal[CAL_TEMP_READ].offset)) / s_cal.cal[CAL_TEMP_READ].gain;

	ADC_CLOCK_SETUP_T adc;
	Chip_ADC_Init(LPC_ADC, &adc);
	s_adc_cr = LPC_ADC->CR &= ~ADC_CR_LPWRMODE;

	sendbytes_itm("\r\nHello from riser!\r\n", 21);
	//printhex_itm("cont:  ", __get_CONTROL());
	printhex_itm("msp:  ", __get_MSP());
	printhex_itm("vtor: ", SCB->VTOR);
	printhex_itm("adc_cr: ", s_adc_cr);

	sendbytes_itm("Module ID: ", 11);
	sendbytes_itm(s_id.model_name, strlen(s_id.model_name));
#if 1
	sendbytes_itm("\r\nCal data:\r\n", 13);
	for (uint32_t i = 0; i < CAL_MAX_VAL; i++) {
		printhex_itm("gain: ", s_cal.cal[i].gain);
		printhex_itm("offs: ", s_cal.cal[i].offset);
	}

	printhex_itm("vset: ", s_setpoint.voltage);
	printhex_itm("iset: ", s_setpoint.current);
	printhex_itm("ovp:  ", s_setpoint.ovp);
	printhex_itm("ocp:  ", s_setpoint.ocp);
#endif

#if 0
	for (uint32_t i = 0; i < 1024; i += 4) {
		uint32_t eebuf = 0x42424242;
		iap_ee_read(i, &eebuf, sizeof(eebuf));
		printhex_itm("EE:", eebuf);
	}
#endif

	Chip_UART_IntEnable(LPC_USART, UART_IER_RBRINT); // Receive fifo level or timeout
	NVIC_EnableIRQ(UART0_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);
	adc_setup_burst(0);

    volatile static int i = 0 ;
    // Enter an infinite loop, just incrementing a counter
    while(1) {
    	__WFI();
    	if (!(i & 0xfffff)) ITM_SendChar('.');
        i++ ;
//        __asm volatile ("nop");
    }
}
