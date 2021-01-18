/*
 * display.c - PS2300 front panel 31337 segment SPI display support
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

/*
The displays seems to use some variant of a Holtek HT1621 controller
https://www.holtek.com/documents/10179/116711/HT1621v321.pdf
SPI comms to displays @max 150kbps MSB first
Controller initialization sequence:
SYS EN
LCD ON
BIAS 1/3 4COM

Display update sequence:
ID = Write (0b101)
Address 0x3f (6 bits) (this way the digits will be 8-bit aligned, see below)
22 nibbles data (lsb first really but we can ignore that, first nibble is unused)

Segment mapping (byte representation):
Byte#  Bit7    6     5     4          3     2     1     0
   0:   [x]   [x]   [x]   [x]   voltFine Preset V/Cring V/C
   1:   OVP  Slash  OCP OCP/OCPring Lock Lockring Remote [x]
   2: Divider Off   On    [x]      Voltage  OT    CC Current
   3:    b     g     c  dp(Fine)      a     f     e     d    (Digit #8, rightmost)
   4:    b     g     c    dp          a     f     e     d    (Digit #7)
   5:    b     g     c    dp          a     f     e     d    (Digit #6)
   6:    b     g     c    dp          a     f     e     d    (Digit #5)
   7:    b     g     c   dp(CV)       a     f     e     d    (Digit #4)
   8:    b     g     c    dp          a     f     e     d    (Digit #3)
   9:    b     g     c    dp          a     f     e     d    (Digit #2)
  10:    b     g     c    dp          a     f     e     d    (Digit #1, leftmost)

  7-segment naming:
  https://en.wikipedia.org/wiki/Seven-segment_display_character_representations
*/

#include "gpiomap.h"
#include "display.h"
#include "stopwatch.h"
#include "font7seg.h"
#include <string.h>

#define HT_ID_LEN (3)
#define HT_ID_CMD (0b100)
#define HT_ID_DATA_WRITE (0b101)

#define HT_CMD_LEN (9)
#define HT_CMD_SYS_EN (0b000000010)
#define HT_CMD_LCD_ON (0b000000110)
#define HT_CMD_1_3_BIAS_4_COM (0b001010010)

#define HT_ADDR_LEN (6)

#define DISP_INIT_LEN (HT_ID_LEN + 3 * HT_CMD_LEN)
static const uint32_t s_disp_init = HT_ID_CMD << (3 * HT_CMD_LEN) |
									HT_CMD_SYS_EN << (2 * HT_CMD_LEN) |
									HT_CMD_LCD_ON << (HT_CMD_LEN) |
									HT_CMD_1_3_BIAS_4_COM;

// Start display write at address 0x3f to make 7seg 8-bit aligned
#define DISP_WR_LEN (HT_ID_LEN + HT_ADDR_LEN)
static const uint16_t s_disp_wr = HT_ID_DATA_WRITE << HT_ADDR_LEN | 0x3f;

static void spi_set_cs_mask( uint8_t cs_mask ) {
	SPI_CS0(1);
	SPI_CS1(1);
	if (cs_mask & 1) SPI_CS0(0);
	if (cs_mask & 2) SPI_CS1(0);
}

static void spi_xfer( uint32_t data, uint8_t bitcount ) {
	if (bitcount > 0 && bitcount <= 32) {
		uint32_t bitmask = 1 << (bitcount - 1);
		do {
			SPI_CLK( 0 );
			SPI_DAT( data & bitmask );
			StopWatch_DelayUs(7);
			SPI_CLK( 1 );
			StopWatch_DelayUs(7);
			bitmask >>= 1;
		} while (bitmask);
	}
}

void disp_init( uint8_t cs_mask ) {
	SPI_RD(1);
	spi_set_cs_mask( 0 );
	StopWatch_DelayUs(1000);

	// Send initialization commands
	spi_set_cs_mask( cs_mask );
	spi_xfer( s_disp_init, DISP_INIT_LEN );
	spi_set_cs_mask( 0 );

	// Light all segments
	uint8_t fill[11];
	memset( fill, 0xff, sizeof(fill));
	StopWatch_DelayUs(7);
	disp_update( cs_mask, fill, 8, sizeof(fill));
}

void disp_update( uint8_t cs_mask, uint8_t* data, uint8_t bitcount, uint8_t bytecount ) {
	spi_set_cs_mask( cs_mask );
	spi_xfer( s_disp_wr, DISP_WR_LEN );
	for (int i = 0; i < bytecount; i++) {
		spi_xfer( data[i], bitcount );
	}
	spi_set_cs_mask( 0 );
}

void disp_font_str( uint8_t* dispbuf, uint8_t startpos, uint8_t numchars, char* text ) {
	for (uint32_t i = 0; i < numchars; i++) {
		// Make sure we stay on the display
		if ((i + startpos) >= 8) break;

		uint32_t dispbuf_offset = 10 - i - startpos;
		uint32_t tmp = text[i];
		// Preserve decimal points / CV / CurrFine indicators occupying bit 4
		uint8_t newdigit = dispbuf[dispbuf_offset] & 0b00010000;

		// Force upper-case
		if (tmp >= 'a' && tmp <= 'z') tmp -= 0x20;

		tmp -= s_font7seg_first_char; // yes, this may wrap (will be dealt with below)
		if (tmp < sizeof(s_font7seg)) {
			newdigit |= s_font7seg[tmp];
		}
		dispbuf[dispbuf_offset] = newdigit;
	}
}

void disp_add_glyph( uint8_t* dispbuf, disp_glyphs_t glyph) {
	if (glyph < GLYPH_MAX) {
		dispbuf[glyph >> 3] |= 1 << (glyph & 0x07);
	}
}

uint32_t disp_format_digits(char* outbuf, int32_t inum, uint32_t mindigits) {
    // Max range -1999 to 9999, but max number of characters output is 3
    // This means that numbers between -1000 and -1999 will have the minus
    // and first digit combined in one character
    // Returns number of characters which will be used even if outbuf is NULL
    // mindigits sets the minimum number of digits output (useful for fixed-
    // point operation)
    if (inum < -1999 || inum > 9999) return 0;
    bool isnegative = inum < 0;
    uint32_t num = isnegative ? -inum : inum;
    uint32_t digits = 1;
    if (num > 9) digits++;
    if (num > 99) digits++;
    if (num > 999) digits++;
    if (digits < mindigits) digits = mindigits;
    uint32_t numchars = (isnegative && digits < 4) ? (digits + 1) : digits;
    uint32_t offset = numchars - digits;
    if (outbuf) {
		uint32_t loop = digits;
		while (loop--) {
			outbuf[loop + offset] = '0' + num % 10;
			num /= 10;
		}
        if (isnegative) outbuf[0] = (outbuf[0] == '1') ? '/' : '-'; // Slash replaced by a merged -1
    }
    return numchars;
}

uint32_t disp_format_hex(char* outbuf, uint32_t bufsize, uint32_t inum, uint32_t mindigits) {
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

// Used by the hard-fault handler among others
void disp_debug(char* leftstr, uint32_t rightnum) {
    uint8_t dispbuf[11];
    char tmpbuf[9];
    memset (dispbuf, 0, sizeof(dispbuf));
    uint32_t numch = 8;
    disp_font_str(dispbuf, 0, numch, leftstr);
    disp_update(DISP_CH1, dispbuf, 8, sizeof(dispbuf));

    memset (dispbuf, 0, sizeof(dispbuf));
    numch = disp_format_hex(tmpbuf, sizeof(tmpbuf), rightnum, 8);
    disp_font_str(dispbuf, 0, numch, tmpbuf);
    disp_update(DISP_CH2, dispbuf, 8, sizeof(dispbuf));
}
