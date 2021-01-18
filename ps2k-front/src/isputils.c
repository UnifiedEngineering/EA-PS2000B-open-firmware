/*
 * isputils.c - LPC ISP mode support
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

#include "isputils.h"
#include "gpiomap.h"
#include "FreeRTOS.h"
#include "task.h"

static uint32_t uuencode(char* dst, uint8_t* src, uint32_t srclen) {
	if (srclen > 45) return 0;

	uint32_t len = 0;
	dst[len++] = 0x20 + srclen;

	for (uint32_t i = 0; i < srclen ; i += 3) {
		uint32_t uugroup = src[i] << 16 |
				((i < (srclen-1)) ? src[i + 1] << 8 : 0) |
				((i < (srclen-2)) ? src[i + 2] : 0);
		for (uint32_t j = 0; j < 4; j++) {
			if (uugroup & (0b111111 << 18)) { // non-zero 6-bit char
				uugroup += (0x20 << 18); // only add 0x20 to the 6-bit char
			} else {
				uugroup += (0x60 << 18); // add 0x60
			}
			dst[len++] = (uint8_t)(uugroup >> 18);
			// Make sure we don't have any left-overs from last byte
			uugroup &= (1 << 18) - 1;
			uugroup <<= 6; // next 6-bit chunk

		}
	}
	dst[len++] = '\r';
	dst[len++] = '\n';
	return len;
}

static uint32_t num2ascii(char* outbuf, uint32_t num) {
	// Returns number of characters which will be used even if outbuf is NULL
	// mindigits sets the minimum number of digits output (useful for fixed-
	// point operation)
	uint32_t digits;
	if (num != 0) {
		uint32_t tmp = num;
		digits = 0;
		while (tmp != 0) {
			tmp /= 10;
			digits++;
		}
	} else {
		digits = 1;
	}

	if (outbuf) {
		uint32_t loop = digits;
		while (loop--) {
			outbuf[loop] = '0' + num % 10;
			num /= 10;
		}
	}
	return digits;
}

extern uint8_t modulefw[];
extern uint32_t modulefwsize;

// Major hack at the moment, needs cleanup
void isp_mode(void) {
	uint8_t* xferstart = modulefw;
	uint32_t xferlen = modulefwsize;
	uint32_t exec = *(uint32_t*)(&modulefw[4]) & ~1; // Remove thumb bit
	uint32_t dest = exec & ~0xff; // hack using reset vector of the image (assuming resetisr is located less than 0xff bytes from the start)

	for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
		CHx_RESET(chnum, 0);
		// Set ISP pin low (request ISP mode)
		CHx_ISP(chnum, 0);
		vTaskDelay(1);
		// Set reset pin high to bring module out of reset
		CHx_RESET(chnum, 1);

		Chip_UART_SetBaud(CHx_UART(chnum), 115200);
	}

	vTaskDelay(310); // Reset supervisor requires this HUMONGOUS delay

	char isptmp[72];

	for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
		// Make sure no garbage is in the UART FIFO
		Chip_UART_SetupFIFOS(CHx_UART(chnum), (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS));

		Chip_UART_SendBlocking(CHx_UART(chnum), "?", 1);
		// Wait for "Synchronized\r\n" (14 bytes)
		uint32_t num = 0;
		while (num < 14) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}

		// RIGHT BACK AT YA! (note that this gets echoed back and this echo should be discarded)
		Chip_UART_SendBlocking(CHx_UART(chnum), isptmp, 14);
		// Wait for "OK\r\n" (4 bytes)
		num = 0;
		while (num < 4) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}

		// Send crystal frequency in khz (unused for this part, this cmd is also echoed)
		Chip_UART_SendBlocking(CHx_UART(chnum), "12000\r\n", 7);
		// Wait for "OK\r\n" (4 bytes)
		num = 0;
		while (num < 4) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}

		// Send echo off cmd (this is echoed)
		Chip_UART_SendBlocking(CHx_UART(chnum), "A 0\r\n", 5);
		// Wait for "0\r\n" (3 bytes)
		num = 0;
		while (num < 3) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}
	}

	vTaskDelay(1);

	for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
		// Make sure no garbage is in the UART FIFO
		Chip_UART_SetupFIFOS(CHx_UART(chnum), (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS));

		// Send unlock cmd (finally no echo!)
		Chip_UART_SendBlocking(CHx_UART(chnum), "U 23130\r\n", 9);
		// Wait for "0\r\n" (3 bytes)
		uint32_t num = 0;
		while (num < 3) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}

		// Send write RAM cmd (destination and length derived from reset vector in module blob)
		num = 0;
		isptmp[num++] = 'W';
		isptmp[num++] = ' ';
		num += num2ascii(&isptmp[num], dest);
		isptmp[num++] = ' ';
		num += num2ascii(&isptmp[num], xferlen);
		isptmp[num++] = '\r';
		isptmp[num++] = '\n';
		Chip_UART_SendBlocking(CHx_UART(chnum), isptmp, num);
		// Wait for "0\r\n" (3 bytes)
		num = 0;
		while (num < 3) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}
	}

	// Send uuencoded data here, checksum after every 20 rows, and at the end
	uint32_t rowcount = 0;
	uint32_t cksumoffset = 0;
	for (uint32_t i = 0; i < xferlen; i += 45) {
		uint32_t len = xferlen - i;
		if (len > 45) {
			len = 45;
			rowcount++;
		} else {
			// Last line (which may or may not be the full 45 encoded bytes long)
			// Force checksum
			rowcount = 20;
		}
		uint32_t uulen = uuencode(isptmp, &xferstart[i], len);
		if (rowcount == 20) {
			// Append checksum to data
			uint32_t cksum = calc_checksum(&xferstart[cksumoffset], len + i - cksumoffset);
			uulen += num2ascii(&isptmp[uulen], cksum);
			isptmp[uulen++] = '\r';
			isptmp[uulen++] = '\n';
			cksumoffset = i + len;
		}
		for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
			Chip_UART_SendBlocking(CHx_UART(chnum), isptmp, uulen);
		}
		if (rowcount == 20) {
			for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
				// Wait for "OK\r\n" (4 bytes)
				uint32_t num = 0;
				while (num < 4) {
					num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
					vTaskDelay(1);
				}
			}
			rowcount = 0;
		}
	}

	for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
		// Send go cmd (jump to the reset vector, thumb mode)
		uint32_t num = 0;
		isptmp[num++] = 'G';
		isptmp[num++] = ' ';
		num += num2ascii(&isptmp[num], exec);
		isptmp[num++] = ' ';
		isptmp[num++] = 'T';
		isptmp[num++] = '\r';
		isptmp[num++] = '\n';
		Chip_UART_SendBlocking(CHx_UART(chnum), isptmp, num);
		// Wait for "0\r\n" (3 bytes)
		num = 0;
		while (num < 3) {
			num += Chip_UART_Read(CHx_UART(chnum), &isptmp[num], sizeof(isptmp) - num);
			vTaskDelay(1);
		}

		// ISP mode done, back to the 500kbps for regular operation
		Chip_UART_SetBaud(CHx_UART(chnum), 500000);
	}

	for (uint32_t chnum = 0; chnum < NUM_CHANNELS; chnum++) {
		// Make sure no garbage is in the UART FIFO
		Chip_UART_SetupFIFOS(CHx_UART(chnum), (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS));
	}
}

uint32_t calc_checksum(uint8_t* buf, uint32_t size) {
	uint32_t result = 0;
	for (int i = 0; i < size; i++) {
		result += buf[i];
	}
	return result;
}
