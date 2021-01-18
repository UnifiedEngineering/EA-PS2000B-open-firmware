#ifndef DISPLAY_H_
#define DISPLAY_H_

#define DISP_CH1 (0b01)
#define DISP_CH2 (0b10)
#define DISP_BOTH (0b11)

// See segment mapping in display.c for details
typedef enum {
	GLYPH_V_SLASH_C = 0,
	GLYPH_V_SLASH_C_RING,
	GLYPH_PRESET,
	GLYPH_VOLT_FINE,

	// unused segment and padding here
	GLYPH_REMOTE = 9,
	GLYPH_LOCK_RING,
	GLYPH_LOCK,
	GLYPH_OVP_SLASH_OCP_RING,
	GLYPH_OCP,
	GLYPH_SLASH,
	GLYPH_OVP,

	GLYPH_CURRENT = 16,
	GLYPH_CC,
	GLYPH_OT,
	GLYPH_VOLTAGE,
	// unused segment here
	GLYPH_ON = 21,
	GLYPH_OFF,
	GLYPH_DIVIDER_LINE,

	GLYPH_CURR_FINE = 28,
	GLYPH_DP7 = 36, // DP after 7th digit
	GLYPH_DP6 = 44, // DP after 6th digit
	GLYPH_DP5 = 52, // DP after 5th digit
	GLYPH_CV = 60,
	GLYPH_DP3 = 68, // DP after 3rd digit
	GLYPH_DP2 = 76, // DP after 2nd digit
	GLYPH_DP1 = 84, // DP after 1st digit

	GLYPH_MAX
} disp_glyphs_t;

void disp_init( uint8_t cs_mask );
void disp_update( uint8_t cs_mask, uint8_t* data, uint8_t bitcount, uint8_t bytecount );
void disp_font_str( uint8_t* dispbuf, uint8_t startpos, uint8_t numchars, char* text );
void disp_add_glyph( uint8_t* dispbuf, disp_glyphs_t glyph);
uint32_t disp_format_digits(char* outbuf, int32_t inum, uint32_t mindigits);
uint32_t disp_format_hex(char* outbuf, uint32_t bufsize, uint32_t inum, uint32_t mindigits);
void disp_debug(char* leftstr, uint32_t rightnum);

#endif /* DISPLAY_H_ */
