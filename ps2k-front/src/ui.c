/*
 * ui.c - Front panel UI
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

#include "ui.h"
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include "gpiomap.h"
#include "keypad.h"
#include "display.h"
#include "powersupply.h"

#define ONE_STEP (50)
#define TIMER_SHORT_PRESET (1000 / ONE_STEP)
#define TIMER_LONG_PRESET (5000 / ONE_STEP)

typedef enum {
	DISP_READBACK = 0, // Default state showing actual readback values
	DISP_SETPOINT, // Show setpoint values (during adjust)
	DISP_PRESET, // Show setpoint values (when pressing Preset)
	DISP_STATUS,
	DISP_MAX_VAL
} disp_t;

typedef struct {
	uint32_t volt_setpoint;
	uint32_t curr_setpoint;
	bool onoff;
	bool lastonoff;
	disp_t disp;
	uint32_t disp_timer_restart;
	uint32_t disp_timer;
} ch_state_t;

typedef struct {
	key_t onoff_key;
	key_t preset_key;
	key_t volt_key;
	key_t curr_key;
	encoder_t volt_enc;
	encoder_t curr_enc;
} ch_cfg_t;

static const ch_cfg_t s_ch_cfg[NUM_CHANNELS] = {
	{
		.onoff_key = KEY_CH1_ON_OFF,
		.preset_key = KEY_CH1_PRESET,
		.volt_key = KEY_CH1_VOLT,
		.curr_key = KEY_CH1_CURR,
		.volt_enc = ENC_CH1_VOLT,
		.curr_enc = ENC_CH1_CURR
	},
	{
		.onoff_key = KEY_CH2_ON_OFF,
		.preset_key = KEY_CH2_PRESET,
		.volt_key = KEY_CH2_VOLT,
		.curr_key = KEY_CH2_CURR,
		.volt_enc = ENC_CH2_VOLT,
		.curr_enc = ENC_CH2_CURR
	}
};

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
	if (delta > 30 || delta < -30) delta *= 5;

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

// Experimental front panel "UI".
void ui_task( void* pvParameters ) {
	const conversion_info_t* scale = ps_get_conv_info_ptr();

    uint8_t dispbuf[11];
    char tmpbuf[9];

    vTaskDelay(1000);
    memset (dispbuf, 0, sizeof(dispbuf));
    disp_font_str(dispbuf, 0, 8, "x UEoS x");
    disp_update(DISP_CH1, dispbuf, 8, sizeof(dispbuf));
    vTaskDelay(1000);
    LED_TRACKING(false);

    ch_state_t ch_state[NUM_CHANNELS];
    memset (ch_state, 0, sizeof(ch_state));

    while(1) {
		static bool tracking = false;

		key_t newkeys = get_keys_pressed();
		if (newkeys & KEY_TRACKING) {
			if (!tracking) {
				// Only activate tracking if all outputs are off
				bool tracking_ok = true;
				for (uint32_t ch = 0; ch < NUM_CHANNELS; ch++) {
					if (ch_state[ch].onoff) tracking_ok = false;
				}
				tracking = tracking_ok;
			} else {
				tracking = false;
			}
		}
		LED_TRACKING(tracking);

		uint32_t maxpower = scale[CONVERSION_POWER].max_setpoint;

		for (uint32_t ch = 0; ch < NUM_CHANNELS; ch++) {
			bool voltincreased = false;
			bool changed = false;
			// This needs to be reworked to handle over-voltage/current settings
			int32_t newvolt = handle_encoder(CONVERSION_VOLTAGE, s_ch_cfg[ch].volt_enc, ch, &voltincreased, &changed);
			int32_t newcurr = handle_encoder(CONVERSION_CURRENT, s_ch_cfg[ch].curr_enc, ch, NULL, &changed);
			if (ch && tracking) {
				ch_state[ch].volt_setpoint = ch_state[0].volt_setpoint;
				ch_state[ch].curr_setpoint = ch_state[0].curr_setpoint;
				ch_state[ch].onoff = ch_state[0].onoff;
			} else {
				if (changed) {
					if (ch_state[ch].disp != DISP_PRESET) {
						ch_state[ch].disp_timer = TIMER_SHORT_PRESET;
						ch_state[ch].disp = DISP_SETPOINT;
					} else {
						ch_state[ch].disp_timer = TIMER_LONG_PRESET;
					}
				}
				uint32_t newpower = newvolt * newcurr;
				if (newpower > maxpower) {
					if (voltincreased) { // Reduce current as the user wanted to increase voltage
						newcurr = maxpower / newvolt;
					} else { // Reduce voltage as user wanted to increase current
						newvolt = maxpower / newcurr;
					}
				}
				if (newkeys & s_ch_cfg[ch].onoff_key) ch_state[ch].onoff = !ch_state[ch].onoff;
				ch_state[ch].volt_setpoint = newvolt;
				ch_state[ch].curr_setpoint = newcurr;

				if (newkeys & s_ch_cfg[ch].preset_key) {
					switch (ch_state[ch].disp) {
					case DISP_PRESET:
						ch_state[ch].disp = DISP_READBACK;
						break;
					default:
						ch_state[ch].disp = DISP_PRESET;
						ch_state[ch].disp_timer = TIMER_LONG_PRESET;
					}
				}
			}
			ps_set_setpoints(ch, ch_state[ch].volt_setpoint, ch_state[ch].curr_setpoint, ch_state[ch].onoff);
		}

		vTaskDelay(ONE_STEP);

		for (uint32_t ch = 0; ch < NUM_CHANNELS; ch++) {
			uint32_t rbvolt = ps_get_readback(ch, CONVERSION_VOLTAGE);
			uint32_t rbcurr = ps_get_readback(ch, CONVERSION_CURRENT);
			uint32_t chstatus = ps_get_status(ch);
			memset (dispbuf, 0, sizeof(dispbuf));
			memset (tmpbuf, 0, sizeof(tmpbuf));
			if (chstatus != 0xffffffff) {
				// Major hack here as the first response after an on/off toggle we'll get
				// from the original module firmware will always be old on/off information
				if (ch_state[ch].onoff == ch_state[ch].lastonoff) {
					ch_state[ch].onoff = chstatus & 1;
				} else {
					ch_state[ch].lastonoff = ch_state[ch].onoff;
				}

				uint32_t leftval, rightval, leftmindigits, rightmindigits;
				switch(ch_state[ch].disp) {
				case DISP_READBACK:
					leftval = rbvolt;
					leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
					rightval = rbcurr;
					rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
					break;
				case DISP_SETPOINT:
				case DISP_PRESET:
					leftval = ch_state[ch].volt_setpoint;
					leftmindigits = scale[CONVERSION_VOLTAGE].num_decimals + 1;
					rightval = ch_state[ch].curr_setpoint;
					rightmindigits = scale[CONVERSION_CURRENT].num_decimals + 1;
					break;
				case DISP_STATUS:
					leftval = chstatus >> 8;
					rightval = chstatus & 0xff;
					leftmindigits = rightmindigits = 1;
				default:
					leftval = rightval = leftmindigits = rightmindigits = 0;
				}
				uint32_t numch = disp_format_digits(NULL, leftval, leftmindigits);
				numch = disp_format_digits(&tmpbuf[4 - numch], leftval, leftmindigits);
				numch = disp_format_digits(NULL, rightval, rightmindigits);
				numch = disp_format_digits(&tmpbuf[8 - numch], rightval, rightmindigits);
				numch = 8;
				disp_font_str(dispbuf, 0, numch, tmpbuf);
				switch(ch_state[ch].disp) {
				case DISP_SETPOINT:
				case DISP_PRESET:
					disp_add_glyph(dispbuf, GLYPH_PRESET);
					// Fallback intentional
				case DISP_READBACK:
					disp_add_glyph(dispbuf, left_dp[ scale[CONVERSION_VOLTAGE].num_decimals ]);
					disp_add_glyph(dispbuf, GLYPH_VOLTAGE);
					disp_add_glyph(dispbuf, right_dp[ scale[CONVERSION_CURRENT].num_decimals ]);
					disp_add_glyph(dispbuf, GLYPH_CURRENT);
					break;
				case DISP_STATUS:
				default:
					;
				}
				disp_add_glyph(dispbuf, GLYPH_DIVIDER_LINE);
				if (ch_state[ch].disp == DISP_READBACK) {
					if (chstatus & 1) disp_add_glyph(dispbuf, (chstatus & STATUS_MODE_MASK) == STATUS_CC ? GLYPH_CC : GLYPH_CV);
				}
				disp_add_glyph(dispbuf, chstatus & STATUS_OUTPUT_ON ? GLYPH_ON : GLYPH_OFF);
				disp_add_glyph(dispbuf, chstatus & STATUS_OVERTEMP ? GLYPH_OT : GLYPH_MAX);
				if (ch && tracking) {
					disp_add_glyph(dispbuf, GLYPH_LOCK);
				}
			} else {
				// No module communication
				disp_font_str(dispbuf, 0, 8, "--------");
				ch_state[ch].lastonoff = ch_state[ch].onoff = false;
			}
			disp_update(ch ? DISP_CH2 : DISP_CH1, dispbuf, 8, sizeof(dispbuf));

			if (ch_state[ch].disp_timer > 0) {
				ch_state[ch].disp_timer--;
			} else {
				ch_state[ch].disp = DISP_READBACK;
			}
		}
    }
}
