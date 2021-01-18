/*
 * powersupply.c - PS2000_LT Power supply riser USART interface / unit conversion
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
#include "powersupply.h"
#include "FreeRTOS.h"
#include "task.h"
#include "isputils.h"

static bool s_is_isp = false;
static conversion_info_t s_psdata[CONVERSION_MAX_VAL];

typedef struct {
	uint32_t status;
	uint32_t volt_readback_percent;
	uint32_t curr_readback_percent;
	uint32_t volt_setpoint;
	uint32_t curr_setpoint;
	uint8_t rxbuf[16];
	uint8_t numrx;
} chinfo_rw_t;

static chinfo_rw_t s_chinfo_rw[NUM_CHANNELS];

static void uart_setup(uint32_t chnum, uint32_t baudrate) {
	LPC_USART_T* pUART = CHx_UART(chnum);
	Chip_UART_Init(pUART);
	Chip_UART_SetBaud(pUART, baudrate);
	Chip_UART_TXEnable(pUART);

	s_chinfo_rw[chnum].numrx = 0;
}

static uint32_t get_num_decimals( float nominal ) {
	uint32_t num_decimals = 0; // No decimals, good for up to 9999V
	if (nominal < 1000.0f) num_decimals++;
	if (nominal < 100.0f) num_decimals++;
	if (nominal < 10.0f) num_decimals++;
	return num_decimals;
}

static uint32_t num_dec_2_scale_factor(uint32_t num_decimals) {
	uint32_t result = 1;
	while (num_decimals > 0) {
		result *= 10;
		num_decimals--;
	}
	return result;
}

static uint32_t get_readback_multiplier( float nominal, uint32_t num_decimals) {
	// To convert from 1/256% units to display value
	// input * multiplier >> 16
	// where multiplier is ((nominal * scale_factor) * 65536) / 25600
	return (uint32_t)(((65536.0f * nominal * (float)num_dec_2_scale_factor(num_decimals)) / 25600.0f) + 0.5f);
}

static uint32_t get_setpoint_multiplier( float nominal, uint32_t num_decimals) {
	// To convert from display value to 1/256% units for the module
	// setpoint * multiplier >> 16
	// where multiplier is 65536 * 25600 / (nominal * scale_factor)
	return (uint32_t)((65536.0f * 25600.0f / (nominal * (float)num_dec_2_scale_factor(num_decimals))) + 0.5f);
}

static void populate_psdata(float nominal, conversions_t id) {
	switch (id) {
	case CONVERSION_VOLTAGE:
	case CONVERSION_CURRENT:
		s_psdata[id].num_decimals = get_num_decimals(nominal);
		s_psdata[id].max_setpoint = (uint32_t)((nominal * (float)num_dec_2_scale_factor(s_psdata[id].num_decimals)) + 0.5f);
		s_psdata[id].mult_readback = get_readback_multiplier(nominal, s_psdata[id].num_decimals);
		s_psdata[id].mult_setpoint = get_setpoint_multiplier(nominal, s_psdata[id].num_decimals);
		break;
	case CONVERSION_POWER:
		s_psdata[id].num_decimals = s_psdata[CONVERSION_VOLTAGE].num_decimals + s_psdata[CONVERSION_CURRENT].num_decimals;
		s_psdata[id].max_setpoint = (uint32_t)((nominal * (float)num_dec_2_scale_factor(s_psdata[id].num_decimals)) + 0.5f);
		s_psdata[id].mult_readback = 0;
		s_psdata[id].mult_setpoint = 0;
		break;
	default:
		;
	}
}

static uint32_t ps_display_to_percent_setpoint(uint32_t disp, conversions_t type) {
	return ((s_psdata[type].mult_setpoint * disp) + 32768) >> 16;
}

static uint32_t ps_percent_to_display_readback(uint32_t percent, conversions_t type) {
	return ((s_psdata[type].mult_readback * percent) + 32768) >> 16;
}

static uint32_t s_initneeded[NUM_CHANNELS];

static void parse_rxbuf(uint32_t chnum) {
	uint32_t numbytes = s_chinfo_rw[chnum].numrx;
	uint8_t* buf_p = s_chinfo_rw[chnum].rxbuf;
	uint32_t cksum = 0xff & calc_checksum(buf_p, numbytes - 1);
	if ((buf_p[0] & 0xf) == numbytes - 1 && cksum == buf_p[numbytes - 1]) {
		// Parse response
		switch (buf_p[0] & 0xf0) {
		case 0x10:
			s_chinfo_rw[chnum].status = buf_p[6] << 8 | buf_p[1];
			s_chinfo_rw[chnum].volt_readback_percent = buf_p[2] << 8 | buf_p[3];
			s_chinfo_rw[chnum].curr_readback_percent = buf_p[4] << 8 | buf_p[5];
			break;
		case 0x80:
			switch (buf_p[1]) {
			case 0x09:
				s_chinfo_rw[chnum].volt_setpoint = ps_percent_to_display_readback(buf_p[2] << 8 | buf_p[3], CONVERSION_VOLTAGE);
				break;
			case 0x0a:
				s_chinfo_rw[chnum].curr_setpoint = ps_percent_to_display_readback(buf_p[2] << 8 | buf_p[3], CONVERSION_CURRENT);
				break;
			}
			s_initneeded[chnum] &= ~_BV(buf_p[1]);
			break;
		}
		s_chinfo_rw[chnum].numrx = 0;
	}
}

static void ps_send_request(uint32_t chnum, uint32_t objid) {
	if (s_is_isp || chnum >= NUM_CHANNELS) return;

	uint8_t tmpcmd[] = {0x82, objid, 0x00};
	uint32_t size = sizeof(tmpcmd);
	uint8_t cksum = (uint8_t)calc_checksum(tmpcmd, size - 1);
	tmpcmd[size - 1] = cksum;
	s_chinfo_rw[chnum].numrx = 0;
	Chip_UART_SendBlocking(CHx_UART(chnum), tmpcmd, sizeof(tmpcmd));
}

static void ps_task( void* pvParameters ) {
// Either we RAM-load firmware or let the modules boot from internal flash
#if 1
	s_is_isp = true;
	isp_mode();
#else
	for (int i = 0; i < NUM_CHANNELS; i++) {
		// Set reset pin low (reset supervisor will bring reset low immediately)
		CHx_RESET(i, 0);

		// Set ISP pin high (don't request ISP mode)
		CHx_ISP(i, 1);

		vTaskDelay(1);

		// Set reset pin high to bring module out of reset
		// There is up to 300ms delay until the reset circuitry releases reset after this
		CHx_RESET(i, 1);
	}
	vTaskDelay(310);
#endif
	s_is_isp = false;

	while (1) {
		for (int i = 0; i < NUM_CHANNELS; i++) {
			if (s_initneeded[i]) {
				uint32_t tmp = s_initneeded[i];
				uint32_t objid = 0;
				// Find first bit set
				while (objid < 32 && !(tmp & 1)) {
					objid++;
					tmp >>= 1;
				}
				if (objid < 32) {
					ps_send_request(i, objid);
				}
				vTaskDelay(2);
			}
		}
		for (int i = 0; i < NUM_CHANNELS; i++) {
			LPC_USART_T* pUART = CHx_UART(i);

			uint8_t tmp;
			uint32_t num;
			do {
				num = Chip_UART_Read(pUART, &tmp, 1);
				if (!num) num = Chip_UART_Read(pUART, &tmp, 1);
				if (num > 0) {
					if (s_chinfo_rw[i].numrx < 16) {
						s_chinfo_rw[i].rxbuf[s_chinfo_rw[i].numrx++] = tmp;
						if (s_chinfo_rw[i].numrx > 3) {
							parse_rxbuf(i);
						}
					}
				}
			} while (num > 0);
		}
		vTaskDelay(1);
	}
}

void ps_init(void) {
	s_is_isp = true;

	// Determine how to scale 0-25600 values to/from modules from/to real-world values
	// As only 4 digits fits on the display, the number of decimal places to show
	// is determined by the maximum voltage/current. With a scale factor of 1 no decimals
	// are shown, up to a maximum scale factor of 1000 which will display three decimals.

	populate_psdata( FLASH_CAL->FLOAT_NOMINAL_VOLTAGE, CONVERSION_VOLTAGE );
	populate_psdata( FLASH_CAL->FLOAT_NOMINAL_CURRENT, CONVERSION_CURRENT );

	// The maximum power needs to be kept below the nominal value.
	// Calculate the maximum power in display values to easier be able to compare (and limit)
	// by simply and cheaply multiply display values of voltage and current, then compare with
	// this value

	populate_psdata( FLASH_CAL->FLOAT_NOMINAL_POWER, CONVERSION_POWER );

	for (int i = 0; i < NUM_CHANNELS; i++) {
		uart_setup(i, 500000);
		// Set ISP pin high (don't request ISP mode) for now
		CHx_ISP(i, 1);

		// Make sure module stays in reset until we decide what to do with it
		CHx_RESET(i, 0);

		// Request id 9 and 10 from modules (voltage and current setpoints) on startup
		s_initneeded[i] = _BV(9) | _BV(10);
		s_chinfo_rw[i].status = 0xffffffff;
	}

	xTaskCreate( ps_task,            /* The function that implements the task. */
                "ps",                            /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                configMINIMAL_STACK_SIZE,        /* The size of the stack to allocate to the task. */
                NULL,                            /* The parameter passed to the task - not used in this case. */
				configMAX_PRIORITIES - 3, /* The priority assigned to the task. */
                NULL );                          /* The task handle is not required, so NULL is passed. */

}

const conversion_info_t* ps_get_conv_info_ptr(void) {
	return s_psdata;
}

static void ps_set_setpoints_percent(uint32_t chnum, uint32_t voltage, uint32_t current, bool onoff) {
	if (s_is_isp || chnum >= NUM_CHANNELS) return;

	uint8_t tmpcmd[] = {0x16, onoff, voltage >> 8, voltage & 0xff, current >> 8, current & 0xff, 0x00};
	uint32_t size = sizeof(tmpcmd);
	uint8_t cksum = (uint8_t)calc_checksum(tmpcmd, size - 1);
	tmpcmd[size - 1] = cksum;
	s_chinfo_rw[chnum].numrx = 0;
	Chip_UART_SendBlocking(CHx_UART(chnum), tmpcmd, sizeof(tmpcmd));
}

static uint32_t ps_get_readback_percent(uint32_t chnum, conversions_t type) {
	if (chnum >= NUM_CHANNELS) return 0;
	uint32_t result = 0;
	switch (type) {
		case CONVERSION_VOLTAGE:
			result = s_chinfo_rw[chnum].volt_readback_percent;
			break;
		case CONVERSION_CURRENT:
			result = s_chinfo_rw[chnum].curr_readback_percent;
			break;
	}
	return result;
}

void ps_set_setpoints(uint32_t chnum, uint32_t voltage, uint32_t current, bool onoff) {

	s_chinfo_rw[chnum].volt_setpoint = voltage;
	s_chinfo_rw[chnum].curr_setpoint = current;

	uint32_t volt_percent = ps_display_to_percent_setpoint(voltage, CONVERSION_VOLTAGE);
	uint32_t curr_percent = ps_display_to_percent_setpoint(current, CONVERSION_CURRENT);
	ps_set_setpoints_percent(chnum, volt_percent, curr_percent, onoff);
}

uint32_t ps_get_readback(uint32_t chnum, conversions_t type) {
	return ps_percent_to_display_readback(ps_get_readback_percent(chnum, type), type);
}

uint32_t ps_get_setpoint(uint32_t chnum, conversions_t id) {
	switch (id) {
	case CONVERSION_VOLTAGE:
		return s_chinfo_rw[chnum].volt_setpoint;
	case CONVERSION_CURRENT:
		return s_chinfo_rw[chnum].curr_setpoint;
	default:
		return 0;
	}
}

uint32_t ps_get_status(uint32_t chnum) {
	return s_chinfo_rw[chnum].status;
}
