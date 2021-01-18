#ifndef POWERSUPPLY_H_
#define POWERSUPPLY_H_

#include <stdint.h>

typedef enum {
	CONVERSION_VOLTAGE = 0,
	CONVERSION_CURRENT,
	CONVERSION_POWER,
	CONVERSION_MAX_VAL
} conversions_t;

typedef struct {
	uint32_t mult_readback;
	uint32_t mult_setpoint;
	uint32_t max_setpoint; // In display units
	uint32_t num_decimals;
} conversion_info_t;

typedef struct {
	uint32_t unknown; // 0xe2c5c429
	uint32_t unknown2[4]; // offset 0x04 some zeroes and ones
	char model_name[16]; // offset 0x14 "PS 2384-05B"
	char serial_number[16]; // offset 0x24
	char article_number[16]; // offset 0x34 "39200126"
	char unknown3[16]; // offset 0x44 "96265000"
	char firmware_ver_str[16]; // offset 0x54 "V3.06 27.12.18"
	uint32_t unknown4; // offset 0x64 0x00180000
	float FLOAT_NOMINAL_VOLTAGE; // offset 0x68
	float FLOAT_NOMINAL_CURRENT; // offset 0x6c
	float FLOAT_NOMINAL_POWER; // offset 0x70
	uint32_t unknown5; // offset 0x74 0
	uint16_t unknown6[14*6]; // offset 0x78 to 0x1ff a bunch of 0, 1, 10, 100, 1000 & 4000 numbers
} flash_cal;

#define FLASH_CAL ((flash_cal*)0xe000)

/*
Request 00 (ID?) : "PS2000_LT\0\0\0\0"
Request 01 (?) : 0x01 0x96 406 dec matches "V4.06" version perhaps?
Request 02 (?) : 0x00 0x05
Request 03 (?) : 0x6e 0x00 (this is 1.1*25600 big endian! OVP)
Request 04 (?) : 0x6e 0x00 (OCP)
Request 05 (?) : 0x00
Request 08 (?) : 0x00 (power on/off in bit0, cc operation in bit2?, ot in bit 7?)
Request 09 (?) : 0x0f 0x7a (voltage setpoint in 1/256%?)
Request 0a (?) : 0x12 0x00 (current setpoint in 1/256%?)
Request 0c (?) : 0x00 0x00 0x95 0x6e 0x00 0x00 0x01 0xbb 38254 443 dec voltage setpoint related pc obj 0x50
Request 0d (?) : 0x00 0x00 0x84 0xf4 0x00 0x00 0x01 0xce 34036 462 dec current setpoint related
Request 0e (?) : 0x00 0x00 0x88 0xb8 0x00 0x00 0x00 0x00 35000 0 dec
Request 0f (?) : 0x00 0x00 0x79 0x18 0x00 0x00 0x01 0xd0 31000 464 dec
Request 10 (?) : 0x00 0x00 0xdc 0xd0 0xff 0xff 0xff 0x4f 56528 -177 dec voltage related (ad ch5)
Request 11 (?) : 0x00 0x00 0xfb 0x98 0xff 0xff 0xfc 0x3e 64408 -962  dec current related (ad ch1)
Request 12 (?) : 0x00 0x01 0x0d 0xa0 0x00 0x00 0x00 0x00 69024 0 dec temperature related (ad ch7)
Request 13 (?) : 0x00 0x01 0x00 0x00 0x00 0x00 0x02 0x00 65536 512 dec ad ch0 related
Request 14 (?) : 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0 0 dec this is related to ad ch6 but not saved in caldata in ee pc obj 0x58
Request 15 (?) : 0x10 0xbe 4286 dec (max power?)
 */

#define STATUS_OVERTEMP _BV(7)
#define STATUS_MODE_MASK (_BV(2) | _BV(1))
#define STATUS_CC _BV(2)
#define STATUS_OUTPUT_ON _BV(0)

void ps_init(void);
const conversion_info_t* ps_get_conv_info_ptr(void);
void ps_set_setpoints(uint32_t chnum, uint32_t voltage, uint32_t current, bool onoff);
uint32_t ps_get_readback(uint32_t chnum, conversions_t type);
uint32_t ps_get_setpoint(uint32_t chnum, conversions_t type);
uint32_t ps_get_status(uint32_t chnum);

#endif /* POWERSUPPLY_H_ */
