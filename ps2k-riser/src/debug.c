/*
 * debug.c - Debug functions using SWO as an additional UART
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

#include "chip.h"
#include "debug.h"
#include <string.h>

uint32_t format_hex(char* outbuf, uint32_t bufsize, uint32_t inum, uint32_t mindigits) {
	uint32_t digits = 1;
	uint32_t tmp = inum >> 4;
	while (tmp != 0) {
		digits++;
		tmp >>= 4;
	}
	if (digits < mindigits) digits = mindigits;
	if (digits >= bufsize) return 0;
	outbuf[digits] = '\0'; // Terminate
	tmp = digits;

	while (tmp) {
		uint32_t digit = inum & 0xf;
		inum >>= 4;
		outbuf[--tmp] = (digit <= 9) ? digit + '0' : digit + 'a' - 10;
	}
	return digits;
}

void sendbytes_itm(char* text, uint32_t len) {
	while (len--) {
		ITM_SendChar(*text++);
	}
}

void printhex_itm(char* text, uint32_t hex) {
	sendbytes_itm(text, strlen(text));
	char buf[9];
	uint32_t len = format_hex(buf, sizeof(buf), hex, 8);
	sendbytes_itm(buf, len);
	sendbytes_itm("\r\n", 2);
}

void init_swo(void) {
	// 115k2 for now
	// Most likely there will be a null byte output (trace ch id) in between every payload byte)
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable access to registers
	DWT->CTRL = 0x400003FE; // DWT needs to provide sync for ITM
	ITM->LAR = 0xC5ACCE55; // Allow access to the Control Register
	ITM->TPR = ITM_TPR_PRIVMASK_Msk; // Trace access privilege all access
//	ITM->TCR = 0x0001000D; // hmm no swo enabled??
//	ITM->TCR = 0x0001000f; // hmm with swo enabled??
//	ITM->TCR = 1 << ITM_TCR_TraceBusID_Pos | ITM_TCR_DWTENA_Msk | ITM_TCR_TSENA_Msk | ITM_TCR_ITMENA_Msk;

	TPI->ACPR = 625; // bitrate = FCPU / (ACPR + 1)
	TPI->SPPR = 2; // SerialWire Output NRZ (UART) mode
	TPI->FFCR = 0; // Bypass the TPIU formatter and send output directly

	ITM->TCR = ITM_TCR_TraceBusID_Msk | ITM_TCR_ITMENA_Msk | ITM_TCR_SWOENA_Msk | ITM_TCR_SYNCENA_Msk;
	ITM->TER = 1; // Only Enable stimulus port #0 (the one used by ITM_SendChar)

	// Make sure the SWO pin has been high long enough for the UART to synchronize
	ITM_SendChar('\0');
	uint32_t busywait = 100000;
	while(busywait--) {
		__asm volatile ("nop");
	}
}
