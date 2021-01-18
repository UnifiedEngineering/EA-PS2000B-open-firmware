/*
 * keypad.c - PS2300 front panel rotary encoders and buttons handled here
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

#include "gpiomap.h"
#include "keypad.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

// Pack gpio port and bit number into bytes pppbbbbb
#define PACK(x, y) (y | (x << 5))
#define KEY(k) PACK(k##_PORT, k##_BIT)
#define GET_PORT_NUM(x) (x >> 5)
#define GET_BIT_VAL(x) (1 << (x & ((1 << 5) - 1)))

static const uint8_t s_gpio2key[] = { KEY(BTN_CH1_PRESET), KEY(BTN_CH1_ON_OFF), KEY(BTN_CH1_VOLT), KEY(BTN_CH1_CURR),
		KEY(BTN_CH2_PRESET), KEY(BTN_CH2_ON_OFF), KEY(BTN_CH2_VOLT), KEY(BTN_CH2_CURR),
		KEY(BTN_TRACKING)};

#define NUM_KEYS (sizeof(s_gpio2key) / sizeof(s_gpio2key[0]))
static key_t s_keys_down = 0;
static key_t s_keys_pressed = 0;

// 0bxxxsssab
// Number of quartersteps in bit 7-2 (signed, only -3 to 3 ever stored)
// old pin state in bit 1-0
int8_t encoderstate[4];
int32_t encodercounter[4];

static void quadrature_decode(encoder_t encodernum, uint32_t newstate) {
	int32_t oldstate = encoderstate[encodernum];
	newstate &= 0b11;
	uint32_t tmp = (oldstate ^ newstate) & 0b111;
	// Bail if no change or noise
	if (tmp != 0b010 && tmp != 0b101 && tmp != 0b001 && tmp != 0b110) return;

	oldstate &= ~0b11;
	oldstate |= newstate;

	// Invert logic on odd quarter-steps
	if (tmp == 0b010 || tmp == 0b101) { // CW
		oldstate += (1 << 2); // Add one quarter step
	} else if  (tmp == 0b001 || tmp == 0b110) { // CCW
		oldstate -= (1 << 2); // Subtract one quarter step
	}
	if (oldstate < -12 || oldstate >= 16) { // Full step performed
		encodercounter[encodernum] += (oldstate < 0) ? -1 : 1;
		oldstate = 0;
	}
	encoderstate[encodernum] = (int8_t)oldstate;
}

int32_t get_encoder_delta(encoder_t encodernum) {
	taskENTER_CRITICAL();
	int32_t delta = encodercounter[encodernum];
	encodercounter[encodernum] = 0;
	taskEXIT_CRITICAL();
	return delta;
}

key_t get_keys_pressed(void) {
	taskENTER_CRITICAL();
	key_t delta = s_keys_pressed;
	s_keys_pressed = 0;
	taskEXIT_CRITICAL();
	return delta;
}

key_t get_keys_down(void) {
	return s_keys_down;
}

// GPIO interrupts
void EINT3_IRQHandler(void) {
	uint32_t gpio[3];
	uint32_t tmp;
	bool int_pend = true;

	while (int_pend) {
		int_pend = false;
		tmp = LPC_GPIOINT->IO0.STATF;
		tmp |= LPC_GPIOINT->IO0.STATR;
		LPC_GPIOINT->IO0.CLR = tmp;
		if (tmp) int_pend = true;

		gpio[0] = LPC_GPIO[0].PIN;

		tmp = LPC_GPIOINT->IO2.STATF;
		tmp |= LPC_GPIOINT->IO2.STATR;
		LPC_GPIOINT->IO2.CLR = tmp;
		if (tmp) int_pend = true;

		gpio[2] = LPC_GPIO[2].PIN;

		uint32_t enc;
		enc = ((!(gpio[ENC_B_CH1_VOLT_PORT] & _BV(ENC_B_CH1_VOLT_BIT))) |
				((!(gpio[ENC_A_CH1_VOLT_PORT] & _BV(ENC_A_CH1_VOLT_BIT))) << 1));
		quadrature_decode( ENC_CH1_VOLT, enc );

		enc = ((!(gpio[ENC_B_CH1_CURR_PORT] & _BV(ENC_B_CH1_CURR_BIT))) |
				((!(gpio[ENC_A_CH1_CURR_PORT] & _BV(ENC_A_CH1_CURR_BIT))) << 1));
		quadrature_decode( ENC_CH1_CURR, enc );

		enc = ((!(gpio[ENC_B_CH2_VOLT_PORT] & _BV(ENC_B_CH2_VOLT_BIT))) |
				((!(gpio[ENC_A_CH2_VOLT_PORT] & _BV(ENC_A_CH2_VOLT_BIT))) << 1));
		quadrature_decode( ENC_CH2_VOLT, enc );

		enc = ((!(gpio[ENC_B_CH2_CURR_PORT] & _BV(ENC_B_CH2_CURR_BIT))) |
				((!(gpio[ENC_A_CH2_CURR_PORT] & _BV(ENC_A_CH2_CURR_BIT))) << 1));
		quadrature_decode( ENC_CH2_CURR, enc );
	}

// This will require some additional debounce for things to be 100%
	key_t newkeys = 0;
	for (uint32_t i = 0; i < NUM_KEYS; i++) {
		uint32_t tmp = s_gpio2key[i];
		if (!(gpio[GET_PORT_NUM(tmp)] & GET_BIT_VAL(tmp))) newkeys |= 1 << i;
	}
	s_keys_pressed |= newkeys & (newkeys ^ s_keys_down);
	s_keys_down = newkeys;
}

void keypad_init(void) {

	uint32_t int_en[5]; // GPIO0-4 just in case
	memset( int_en, 0, sizeof(int_en));

	for (int i = 0; i < sizeof(encoderstate); i++) {
		encoderstate[i] = 0b00; // Detent at 0b00 after gpio invert
	}
	int_en[BTN_TRACKING_PORT] |= _BV(BTN_TRACKING_BIT);
	int_en[BTN_CH1_PRESET_PORT] |= _BV(BTN_CH1_PRESET_BIT);
	int_en[BTN_CH2_PRESET_PORT] |= _BV(BTN_CH2_PRESET_BIT);
	int_en[BTN_CH1_ON_OFF_PORT] |= _BV(BTN_CH1_ON_OFF_BIT);
	int_en[BTN_CH2_ON_OFF_PORT] |= _BV(BTN_CH2_ON_OFF_BIT);
	int_en[ENC_A_CH1_VOLT_PORT] |= _BV(ENC_A_CH1_VOLT_BIT);
	int_en[ENC_A_CH2_VOLT_PORT] |= _BV(ENC_A_CH2_VOLT_BIT);
	int_en[ENC_B_CH1_VOLT_PORT] |= _BV(ENC_B_CH1_VOLT_BIT);
	int_en[ENC_B_CH2_VOLT_PORT] |= _BV(ENC_B_CH2_VOLT_BIT);
	int_en[BTN_CH1_VOLT_PORT] |= _BV(BTN_CH1_VOLT_BIT);
	int_en[BTN_CH2_VOLT_PORT] |= _BV(BTN_CH2_VOLT_BIT);
	int_en[ENC_A_CH1_CURR_PORT] |= _BV(ENC_A_CH1_CURR_BIT);
	int_en[ENC_A_CH2_CURR_PORT] |= _BV(ENC_A_CH2_CURR_BIT);
	int_en[ENC_B_CH1_CURR_PORT] |= _BV(ENC_B_CH1_CURR_BIT);
	int_en[ENC_B_CH2_CURR_PORT] |= _BV(ENC_B_CH2_CURR_BIT);
	int_en[BTN_CH1_CURR_PORT] |= _BV(BTN_CH1_CURR_BIT);
	int_en[BTN_CH2_CURR_PORT] |= _BV(BTN_CH2_CURR_BIT);

	// Only port 0 and 2 are interrupt capable
	LPC_GPIOINT->IO0.ENF = int_en[0];
	LPC_GPIOINT->IO0.ENR = int_en[0];
	LPC_GPIOINT->IO2.ENF = int_en[2];
	LPC_GPIOINT->IO2.ENR = int_en[2];

	NVIC_SetPriority(EINT3_IRQn, 2);
	NVIC_EnableIRQ(EINT3_IRQn);
}
