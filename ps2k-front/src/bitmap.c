/*
 * bitmap.c - Bitmap rendering of segment display buffer
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

#include <stdint.h>
#include <stdbool.h>
#include "bitmap.h"

//#define HOST_TEST

#ifdef HOST_TEST
#include <arpa/inet.h>
#define __REV(x) htonl(x)

#include <stdio.h>
static void outputbytes(const uint8_t* buf, uint32_t len) {
	for (uint32_t i = 0; i < len; i++) {
//		printf("%c", buf[i]);
		printf("%02x ", buf[i]);
	}
}

int main(void) {

	// Test segment display buffer to render
	uint8_t dispbuf[] = {0xff,0xff,0xff,0xb9,0x09,0x09,0x09,0x19,0x09,0x09,0x0f};

	bitmap_render_full(outputbytes, dispbuf);
}
#else
#include "chip.h"
#endif

// Simple bitmap representation of the segment display
// making it possible to produce a "screenshot"
// 152 x 56 pixels rendered

#define X_SIZE (152)
#define Y_SIZE (56)

#define ASCII 1
#define BMP 2
#define PNG 3

//#define TYPE ASCII
//#define TYPE BMP
#define TYPE PNG

// Glyphs are represented with bit 0 being the topmost
// Lines and pixels are 0-based
// Line 41 from pixel 4 to 147 should be set for DIVIDER_LINE

// 243 bytes 8-bit height glyphs
#define SEG_A_OFFSET (0)
#define SEG_A_LEN (12)
#define SEG_D_OFFSET (SEG_A_OFFSET + SEG_A_LEN)
#define SEG_D_LEN (12)
#define SEG_G_OFFSET (SEG_D_OFFSET + SEG_D_LEN)
#define SEG_G_LEN (12)
#define SEG_DP_OFFSET (SEG_G_OFFSET + SEG_G_LEN)
#define SEG_DP_LEN (2)
#define SEG_FINE_OFFSET (SEG_DP_OFFSET + SEG_DP_LEN)
#define SEG_FINE_LEN (13)
#define SEG_VOLTAGE_OFFSET (SEG_FINE_OFFSET + SEG_FINE_LEN)
#define SEG_VOLTAGE_LEN (26)
#define SEG_CV_OFFSET (SEG_VOLTAGE_OFFSET + SEG_VOLTAGE_LEN)
#define SEG_CV_LEN (9)
#define SEG_OT_OFFSET (SEG_CV_OFFSET + SEG_CV_LEN)
#define SEG_OT_LEN (7)
#define SEG_CC_OFFSET (SEG_OT_OFFSET + SEG_OT_LEN)
#define SEG_CC_LEN (7)
#define SEG_CURRENT_OFFSET (SEG_CC_OFFSET + SEG_CC_LEN)
#define SEG_CURRENT_LEN (26)
#define SEG_PRESET_OFFSET (SEG_CURRENT_OFFSET + SEG_CURRENT_LEN)
#define SEG_PRESET_LEN (22)
#define SEG_VC_OFFSET (SEG_PRESET_OFFSET + SEG_PRESET_LEN)
#define SEG_VC_LEN (13)
#define SEG_OVP_OFFSET (SEG_VC_OFFSET + SEG_VC_LEN)
#define SEG_OVP_LEN (13)
#define SEG_SLASH_OFFSET (SEG_OVP_OFFSET + SEG_OVP_LEN)
#define SEG_SLASH_LEN (3)
#define SEG_OCP_OFFSET (SEG_SLASH_OFFSET + SEG_SLASH_LEN)
#define SEG_OCP_LEN (11)
#define SEG_LOCK_OFFSET (SEG_OCP_OFFSET + SEG_OCP_LEN)
#define SEG_LOCK_LEN (15)
#define SEG_REMOTE_OFFSET (SEG_LOCK_OFFSET + SEG_LOCK_LEN)
#define SEG_REMOTE_LEN (25)
#define SEG_ON_OFFSET (SEG_REMOTE_OFFSET + SEG_REMOTE_LEN)
#define SEG_ON_LEN (7)
#define SEG_OFF_OFFSET (SEG_ON_OFFSET + SEG_ON_LEN)
#define SEG_OFF_LEN (9)

static const uint8_t s_glyphs[] = {
		0b01, // Seg A, width 12
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b01,

		0b10, // Seg D, width 12
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b11,
		0b10,

		0b010, // Seg G, width 12
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b111,
		0b010,

		0b11, // Seg DP, width 2
		0b11,

		0b111111, // Fine, width 13
		0b000101,
		0b000001,
		0b000000,
		0b111101,
		0b000000,
		0b111110,
		0b000010,
		0b111100,
		0b000000,
		0b011100,
		0b101010,
		0b001100,

		0b000011, // Voltage, width 26
		0b011100,
		0b100000,
		0b011100,
		0b000011,
		0b000000,
		0b011100,
		0b100010,
		0b011100,
		0b000000,
		0b111111,
		0b000000,
		0b011111,
		0b100010,
		0b000000,
		0b010000,
		0b101010,
		0b111100,
		0b000000,
		0b011100,
		0b10100010,
		0b01111100,
		0b000000,
		0b011100,
		0b101010,
		0b001100,

		0b011110, // CV, width 9
		0b100001,
		0b100001,
		0b000000,
		0b000011,
		0b011100,
		0b100000,
		0b011100,
		0b000011,

		0b011110, // OT, width 7
		0b100001,
		0b011110,
		0b000000,
		0b000001,
		0b111111,
		0b000001,

		0b011110, // CC, width 7
		0b100001,
		0b100001,
		0b000000,
		0b011110,
		0b100001,
		0b100001,

		0b011110, // Current, width 26
		0b100001,
		0b100001,
		0b000000,
		0b011110,
		0b100000,
		0b111110,
		0b000000,
		0b111110,
		0b000100,
		0b000010,
		0b000000,
		0b111110,
		0b000100,
		0b000010,
		0b000000,
		0b011100,
		0b101010,
		0b001100,
		0b000000,
		0b111110,
		0b000010,
		0b111100,
		0b000000,
		0b011111,
		0b100010,

		0b111111, // Preset, width 22
		0b001001,
		0b000110,
		0b000000,
		0b111110,
		0b000100,
		0b000010,
		0b000000,
		0b011100,
		0b101010,
		0b001100,
		0b000000,
		0b101100,
		0b101010,
		0b010010,
		0b000000,
		0b011100,
		0b101010,
		0b001100,
		0b000000,
		0b011111,
		0b100010,

		0b000011, // V/C, width 13
		0b011100,
		0b100000,
		0b011100,
		0b000011,
		0b000000,
		0b110000,
		0b001100,
		0b000011,
		0b000000,
		0b011110,
		0b100001,
		0b100001,

		0b011110, // OVP, width 13
		0b100001,
		0b011110,
		0b000000,
		0b000011,
		0b011100,
		0b100000,
		0b011100,
		0b000011,
		0b000000,
		0b111111,
		0b001001,
		0b000110,

		0b110000, // Slash, width 3
		0b001100,
		0b000011,

		0b011110, // OCP, width 11
		0b100001,
		0b011110,
		0b000000,
		0b011110,
		0b100001,
		0b100001,
		0b000000,
		0b111111,
		0b001001,
		0b000110,

		0b111111, // Lock, width 15
		0b100000,
		0b100000,
		0b000000,
		0b011100,
		0b100010,
		0b011100,
		0b000000,
		0b011100,
		0b100010,
		0b100010,
		0b000000,
		0b111111,
		0b010100,
		0b100010,

		0b111111, // Remote, width 25
		0b001001,
		0b011001,
		0b100110,
		0b000000,
		0b011100,
		0b101010,
		0b001100,
		0b000000,
		0b111110,
		0b000010,
		0b111100,
		0b000010,
		0b111100,
		0b000000,
		0b011100,
		0b100010,
		0b011100,
		0b000000,
		0b011111,
		0b100010,
		0b000000,
		0b011100,
		0b101010,
		0b001100,

		0b011110, // On, width 7
		0b100001,
		0b011110,
		0b000000,
		0b111110,
		0b000010,
		0b111100,

		0b011110, // Off, width 9
		0b100001,
		0b011110,
		0b000000,
		0b111100,
		0b001010,
		0b000000,
		0b111100,
		0b001010,
};

// 73 words 16-bit height glyphs
#define SEG_B_C_OFFSET (0)
#define SEG_B_C_LEN (2)
#define SEG_E_F_OFFSET (SEG_B_C_OFFSET + SEG_B_C_LEN)
#define SEG_E_F_LEN (2)
#define SEG_VC_RING_OFFSET (SEG_E_F_OFFSET + SEG_E_F_LEN)
#define SEG_VC_RING_LEN (17)
#define SEG_OVP_OCP_RING_OFFSET (SEG_VC_RING_OFFSET + SEG_VC_RING_LEN)
#define SEG_OVP_OCP_RING_LEN (33)
#define SEG_LOCK_RING_OFFSET (SEG_OVP_OCP_RING_OFFSET + SEG_OVP_OCP_RING_LEN)
#define SEG_LOCK_RING_LEN (19)

static const uint16_t s_tallglyphs[] = {
		0b011111111110, // Seg B or C, width 2
		0b111111111111,

		0b111111111111, // Seg E or F, width 2
		0b011111111110,

		0b1111111111, // V/C Ring, width 17
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b0000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1111111111,

		0b1111111111, // OVP/OCP Ring, width 33
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b0000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1111111111,

		0b1111111111, // Lock Ring, width 19
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b0000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1000000001,
		0b1111111111,
};

typedef struct __attribute__((packed)) {
	uint8_t offset; // OFFSET
	uint8_t width; // LEN
	uint8_t x; // Bitmap destination (ref top left corner)
	uint8_t y;
} seg_map_t;

// Flag all segments that needs to be fetched from the "tall" list
static const uint8_t s_tallmap[] = {
		0b00000010, // seg 7-0
		0b00010100, // seg 15-8
		0b00000000, // seg 23-16
		0b10100110, // seg 31-14
		0b10100110, // seg 39-32
		0b10100110, // seg 47-40
		0b10100110, // seg 55-48
		0b10100110, // seg 63-56
		0b10100110, // seg 71-64
		0b10100110, // seg 79-72
		0b10100110 // seg 87-80
};

static const seg_map_t s_map[] = {
		{SEG_VC_OFFSET, SEG_VC_LEN, 29, 45}, // Seg 0
		{SEG_VC_RING_OFFSET, SEG_VC_RING_LEN, 27, 43}, // Seg 1 (this is a 16-bit segment)
		{SEG_PRESET_OFFSET, SEG_PRESET_LEN, 4, 45}, // Seg 2
		{SEG_FINE_OFFSET, SEG_FINE_LEN, 4, 32}, // Seg 3
		{0,0,0,0}, // Padding
		{0,0,0,0}, // Padding
		{0,0,0,0}, // Padding
		{0,0,0,0}, // Padding
		{0,0,0,0}, // Padding
		{SEG_REMOTE_OFFSET, SEG_REMOTE_LEN, 99, 45}, // Seg 9
		{SEG_LOCK_RING_OFFSET, SEG_LOCK_RING_LEN, 79, 43}, // Seg 10 (this is a 16-bit segment)
		{SEG_LOCK_OFFSET, SEG_LOCK_LEN, 81, 45}, // Seg 11
		{SEG_OVP_OCP_RING_OFFSET, SEG_OVP_OCP_RING_LEN, 45, 43}, // Seg 12 (this is a 16-bit segment)
		{SEG_OCP_OFFSET, SEG_OCP_LEN, 65, 45}, // Seg 13
		{SEG_SLASH_OFFSET, SEG_SLASH_LEN, 61, 45}, // Seg 14
		{SEG_OVP_OFFSET, SEG_OVP_LEN, 47, 45}, // Seg 15
		{SEG_CURRENT_OFFSET, SEG_CURRENT_LEN, 101, 32}, // Seg 16
		{SEG_CC_OFFSET, SEG_CC_LEN, 84, 32}, // Seg 17
		{SEG_OT_OFFSET, SEG_OT_LEN, 74, 32}, // Seg 18
		{SEG_VOLTAGE_OFFSET, SEG_VOLTAGE_LEN, 25, 32}, // Seg 19
		{0,0,0,0}, // Padding
		{SEG_ON_OFFSET, SEG_ON_LEN, 130, 45}, // Seg 21
		{SEG_OFF_OFFSET, SEG_OFF_LEN, 139, 45}, // Seg 22
		{255,144,4,41}, // Divider line special case seg 23

		{SEG_D_OFFSET, SEG_D_LEN, 135, 28}, // Seg 24 digit 8
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 134, 17}, // Seg 25 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 134, 4}, // Seg 26 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 135, 3}, // Seg 27
		{SEG_FINE_OFFSET, SEG_FINE_LEN, 135, 32}, // Seg 28
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 146, 17}, // Seg 29 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 135, 15}, // Seg 30
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 146, 4}, // Seg 31 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 117, 28}, // Seg 32 digit 7
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 116, 17}, // Seg 33 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 116, 4}, // Seg 34 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 117, 3}, // Seg 35
		{SEG_DP_OFFSET, SEG_DP_LEN, 131, 28}, // Seg 36
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 128, 17}, // Seg 37 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 117, 15}, // Seg 38
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 128, 4}, // Seg 39 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 99, 28}, // Seg 40 digit 6
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 98, 17}, // Seg 41 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 98, 4}, // Seg 42 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 99, 3}, // Seg 43
		{SEG_DP_OFFSET, SEG_DP_LEN, 113, 28}, // Seg 44
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 110, 17}, // Seg 45 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 99, 15}, // Seg 46
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 110, 4}, // Seg 47 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 81, 28}, // Seg 48 digit 5
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 80, 17}, // Seg 49 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 80, 4}, // Seg 50 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 81, 3}, // Seg 51
		{SEG_DP_OFFSET, SEG_DP_LEN, 95, 28}, // Seg 52
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 92, 17}, // Seg 53 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 81, 15}, // Seg 54
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 92, 4}, // Seg 55 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 59, 28}, // Seg 56 digit 4
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 58, 17}, // Seg 57 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 58, 4}, // Seg 58 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 59, 3}, // Seg 59
		{SEG_CV_OFFSET, SEG_CV_LEN, 62, 32}, // Seg 60
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 70, 17}, // Seg 61 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 59, 15}, // Seg 62
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 70, 4}, // Seg 63 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 41, 28}, // Seg 64 digit 3
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 40, 17}, // Seg 65 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 40, 4}, // Seg 66 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 41, 3}, // Seg 67
		{SEG_DP_OFFSET, SEG_DP_LEN, 55, 28}, // Seg 68
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 52, 17}, // Seg 69 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 41, 15}, // Seg 70
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 52, 4}, // Seg 71 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 23, 28}, // Seg 72 digit 2
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 22, 17}, // Seg 73 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 22, 4}, // Seg 74 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 23, 3}, // Seg 75
		{SEG_DP_OFFSET, SEG_DP_LEN, 37, 28}, // Seg 76
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 34, 17}, // Seg 77 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 23, 15}, // Seg 78
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 34, 4}, // Seg 79 (this is a 16-bit segment)

		{SEG_D_OFFSET, SEG_D_LEN, 5, 28}, // Seg 80 digit 1
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 4, 17}, // Seg 81 (this is a 16-bit segment)
		{SEG_E_F_OFFSET, SEG_E_F_LEN, 4, 4}, // Seg 82 (this is a 16-bit segment)
		{SEG_A_OFFSET, SEG_A_LEN, 5, 3}, // Seg 83
		{SEG_DP_OFFSET, SEG_DP_LEN, 19, 28}, // Seg 84
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 16, 17}, // Seg 85 (this is a 16-bit segment)
		{SEG_G_OFFSET, SEG_G_LEN, 5, 15}, // Seg 86
		{SEG_B_C_OFFSET, SEG_B_C_LEN, 16, 4}, // Seg 87 (this is a 16-bit segment)
};
#define NUM_SEG (sizeof(s_map) / sizeof(s_map[0]))

#define MAX_GLYPH_HEIGHT (16)
uint8_t bitmap_render_pixel(uint8_t* dispbuf, uint32_t x, uint32_t y) {
	// Iterate thru all segments, figure out if there's a segment at x,y
	uint32_t bit = 0;

	for (uint32_t i = 0; i < NUM_SEG; i++) {
		// If this segment isn't lit there's no point to map it
		if (!(dispbuf[i >> 3] & 1 << (i & 0x07))) continue;

		uint32_t width = s_map[i].width;
		uint32_t startx = s_map[i].x;
		uint32_t starty = s_map[i].y;
		if (x >= startx && x < (startx + width) && y >= starty && y < (starty + MAX_GLYPH_HEIGHT)) {
			// Figure out if the segment is a tall one
			bool tall = !!(s_tallmap[i >> 3] & 1 << (i & 0x07));

			// Fetch segment pixel data
			uint32_t data;
			uint32_t xoffset = s_map[i].offset;
			if (xoffset == 255) {
				// Special case for divider line to reduce table size
				data = 0b1;
			} else {
				xoffset += x - startx;
				data = tall ? s_tallglyphs[xoffset] : s_glyphs[xoffset];
			}
			bit |= !!(data & (1 << (y - starty)));
		}
	}
	return bit;
}

// BMP definitions (for max 255x255 1bpp pixels image):
#define BMP_HDR_SIZE (62)
#define BMP_LINE_LEN 4 * (uint32_t)((X_SIZE + 31) / 32) // DWORD aligned length
#define BMP_TRAIL_PAD ((BMP_LINE_LEN * 8) - X_SIZE)
#define BMP_SIZE (BMP_HDR_SIZE + Y_SIZE * BMP_LINE_LEN)
const uint8_t g_bmphdr[BMP_HDR_SIZE] = {
		0x42, 0x4D, BMP_SIZE & 0xff, BMP_SIZE >> 8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, BMP_HDR_SIZE, 0x00, 0x00, 0x00, 0x28, 0x00,
		0x00, 0x00, X_SIZE, 0x00, 0x00, 0x00, -Y_SIZE, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x08, 0x00, 0x00, 0x80, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00};

// PNG definitions:
// PNG signature
static const uint8_t pngsig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

// IHDR 1bpp (max 255x255 pixels)
static const uint8_t pngIHDR[] = {'I', 'H', 'D', 'R',
		0x00, 0x00, 0x00, X_SIZE, 0x00, 0x00, 0x00, Y_SIZE,
		0x01/*bpp*/, 0x03/*idx*/, 0x00, 0x00, 0x00
};

// PLTE Palette for 1-bit image
static const uint8_t pngPLTE[] = {'P', 'L', 'T', 'E',
		0x00, 0x00, 0xff,  0xff, 0xff, 0xff // blue, white
};

#define SCAN_LEN (1 + (X_SIZE + 7) / 8) // Filter byte plus bitmap
#define TOTAL_LEN (SCAN_LEN * Y_SIZE)
#define TOTAL_LEN_INV ~TOTAL_LEN
// IDAT prefix (pixel scanlines, adler32 and crc to be added)
static const uint8_t pngIDAT[] = {'I', 'D', 'A', 'T', 0x78, 0x01, 0x01/*Final (only) block, uncompressed*/,
		// These lengths are little-endian
		TOTAL_LEN & 0xff, TOTAL_LEN >> 8,
		TOTAL_LEN_INV & 0xff, TOTAL_LEN_INV >> 8};

// IEND
static const uint8_t pngIEND[] = {'I', 'E', 'N', 'D'};

#define BASE (65521) // Largest prime smaller than 65536 (from rfc1950)
static uint32_t update_adler32(uint32_t adler, uint8_t* buf, uint32_t len) {
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = adler >> 16;
	for (uint32_t i = 0; i < len; i++) {
	  s1 = (s1 + buf[i]) % BASE;
	  s2 = (s2 + s1) % BASE;
	}
	return (s2 << 16) | s1;
}

#define CRCPOLY (0xedb88320)
static uint32_t update_pngcrc(uint32_t accum, const uint8_t* data, uint32_t len) {
   for (uint32_t idx = 0; idx < len; idx++) {
      accum ^= data[idx];
      for (uint32_t i = 0; i < 8; i++) {
          if (accum & 1) {
        	  accum = (accum >> 1) ^ CRCPOLY;
          } else {
              accum >>= 1;
          }
      }
   }
   return ~accum;
}

static uint32_t outputchunk(void (*out)(const uint8_t*, uint32_t), const uint8_t* buf, uint32_t len, uint32_t full_len) {
	uint32_t tmp = __REV(full_len - 4);
	out( (uint8_t*)&tmp, sizeof(tmp) );
	out( buf, len );
	uint32_t retval = update_pngcrc(~0, buf, len);
	if (full_len == len) {
		tmp = __REV( retval );
		out( (uint8_t*)&tmp, sizeof(tmp) );
	}
	return retval;
}

void bitmap_render_full(void (*out)(const uint8_t*, uint32_t), uint8_t* dispbuf) {

#define X_PAD (0)
#if TYPE == PNG
	// Output PNG header
	out(pngsig, sizeof(pngsig));
	outputchunk(out, pngIHDR, sizeof(pngIHDR), sizeof(pngIHDR));
	outputchunk(out, pngPLTE, sizeof(pngPLTE), sizeof(pngPLTE));
	uint32_t crc = outputchunk(out, pngIDAT, sizeof(pngIDAT), sizeof(pngIDAT) + TOTAL_LEN + 4/*adler32*/);
	uint32_t a32 = 1;
#elif TYPE == BMP
	// Output BMP header
	out(g_bmphdr, sizeof(g_bmphdr));
	// Every pixel line must be padded to a 32-bit aligned size
	#undef X_PAD
	#define X_PAD (BMP_TRAIL_PAD)
#endif
	for (uint32_t y = 0; y < Y_SIZE; y++) {
		uint32_t pixelbuf = 0;
#if TYPE == PNG
		out((uint8_t*)&pixelbuf, 1); // Hack for filter type at the beginning of every scanline
		crc = update_pngcrc(~crc, (uint8_t*)&pixelbuf, 1);
		a32 = update_adler32(a32, (uint8_t*)&pixelbuf, 1);
#endif
		for (uint32_t x = 0; x < (X_SIZE + X_PAD); x++) {
			pixelbuf <<= 1;
			pixelbuf |= bitmap_render_pixel(dispbuf, x, y);
#if TYPE == ASCII
			out( (uint8_t*)((pixelbuf & 1) ? "*" : " "), 1);
#else
			if (7 == (x & 7)) {
				out((uint8_t*)&pixelbuf, 1);
#if TYPE == PNG
				crc = update_pngcrc(~crc, (uint8_t*)&pixelbuf, 1);
				a32 = update_adler32(a32, (uint8_t*)&pixelbuf, 1);
#endif
			}
#endif
		}
#if TYPE == ASCII
		out((uint8_t*)"\r\n", 2);
#endif
	}
#if TYPE == PNG
	uint32_t tmp = __REV(a32);
	out((uint8_t*)&tmp, 4); // adler32
	tmp = __REV( update_pngcrc(~crc, (uint8_t*)&tmp, 4) );
	out((uint8_t*)&tmp, 4); // crc for IDAT
	outputchunk(out, pngIEND, sizeof(pngIEND), sizeof(pngIEND));
#endif
}
