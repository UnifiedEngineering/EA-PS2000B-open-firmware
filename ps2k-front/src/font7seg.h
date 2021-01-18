#ifndef FONT7SEG_H_
#define FONT7SEG_H_

// Segment name to bit number mapping
//
//   a3
// f    b
// 2    7
//   g6
// e    c
// 1    5
//   d0   dp4
static const uint8_t s_font7seg_first_char = 0x2d;
static const uint8_t s_font7seg[] = {
	0b01000000, // - (minus)
	0, // no . (use the dedicated decimal point segments where available)
	0b11100000, // Merged -1 instead of /

	0b10101111, // 0
	0b10100000, // 1
	0b11001011, // 2
	0b11101001, // 3
	0b11100100, // 4
	0b01101101, // 5
	0b01101111, // 6
	0b10101100, // 7
	0b11101111, // 8
	0b11101101, // 9
	0, // no :
	0, // no ;
	0, // no <
	0b01000001, // =
	0, // no >
	0, // no ?

	0, // no @
	0b11101110, // A
	0b01100111, // b
	0b00001111, // C
	0b11100011, // d
	0b01001111, // E
	0b01001110, // F
	0b11101101, // g (9)
	0b01100110, // h
	0b00100000, // i
	0b10100011, // J
	0b01101110, // k-ish (h with additional top bar)
	0b00000111, // L
	0b10001101, // M-ish (Upside down U with bar below)
	0b10101110, // N-ish (Upside down U)
	0b01100011, // o

	0b11001110, // P
	0b11101100, // q
	0b01000010, // r
	0b01101101, // S (5)
	0b01000111, // t
	0b10100111, // U
	0b10100111, // V (U)
	0b00101011, // W-ish (u with bar on top)
	0b01001001, // X-ish (three horizontal bars)
	0b11100101, // y
	0b11001011, // Z (2)
};

#endif /* FONT7SEG_H_ */
