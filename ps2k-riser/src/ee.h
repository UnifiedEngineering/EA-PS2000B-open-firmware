#ifndef EE_H_
#define EE_H_

// First byte in ee is 0x01. Everyone gets upset if this is not 0x01. Don't be that guy.

// ee_id struct starts at ee addr 0x01, 0x14 bytes
#define EE_ID_START (0x01)
typedef struct __attribute__((packed)) {
	char model_name[14]; // "PS2000_LT\0\0\0\0\0" accessed in obj 0
	uint16_t revision_maybe; // 5 (PCB marked PS2000_LT/05) accessed in obj 2
	uint16_t firmware_ver_maybe; // 406 decimal accessed in obj 1
	uint16_t max_out_power; // 4286 decimal in the funkiest of funky units (percent^2) accessed in obj 15
	// The 4286 number makes some sense when thinking about the way setpoints are specified
	// This is for a 160W power supply with nominal 84V output voltage and 5A output current.
	// Obviously 84V*5A is way above 160W so some limiting has to be applied.
	// As setpoints are specified in 1/256% (so 25600 "V" corresponds to 84V and 25600 "A"
	// corresponds to 5A then the output power in the unit used above can be calculated like:
	// (V setpoint * I setpoint) >> 16.
	// If the resulting value exceeds max_power then reject the setpoint request.

	// The front panel will do something like this:
	// If a voltage setpoint increase results in this value being exceeded the current setpoint
	// will be reduced to stay within the limit. Same thing if the current setpoint is increased,
	// in that case the voltage setpoint will automatically be reduced to stay below max power.

	// Example for this 84V 5A unit: 84V 1.905A results in 160W
	// The setpoints will be 84/84*25600 == 25600 and 1.905/5*25600 == 9753
	// 25600*9753/65536 == 3810 so the stored value is apparently roughly 10% above

	// Interestingly the module doesn't seem to know or care about the actual voltage and current,
	// only the power limitation (which is in percent * percent)
} ee_id;

typedef struct __attribute__((packed)) {
	uint32_t gain;
	int32_t offset;
} gain_offset;

typedef enum {
	CAL_VOLT_SET = 0,
	CAL_CURR_SET,
	CAL_UNK1_SET,
	CAL_UNK2_SET,
	CAL_VOLT_READ,
	CAL_CURR_READ,
	CAL_TEMP_READ,
	CAL_REF_READ,
	CAL_MAX_VAL
} cal_t;

// ee_cal struct starts at ee addr 0x26, 0x40 bytes
// Entries are accessed in obj 12 (0x0c) to obj 19 (0x13)
#define EE_CAL_START (0x26)
typedef struct __attribute__((packed)) {
	gain_offset cal[CAL_MAX_VAL]; // access in obj 12-19 (0x0c-0x13)
} ee_cal;

// ee_setpoint struct starts at ee addr 0x6e, 0x12 bytes
#define EE_SETPOINT_START (0x6e)
typedef struct __attribute__((packed)) {
	uint16_t unknown5; // 0x0000 one byte access in obj 5
	uint16_t unknown6; // 0x0000 access in obj 6
	uint16_t unknown7; // 0x0000 access in obj 7
	uint16_t unknown8; // 0x0000
	uint16_t voltage; // Voltage setpoint value (in 1/256% units as usual) access in obj 9
	uint16_t current; // Current setpoint value (in 1/256% units as usual) access in obj 10 (0x0a)
	uint16_t ovp; // Over-voltage protection value (in 1/256% units as usual) access in obj 3
	uint16_t ocp; // Over-current protection value (in 1/256% units as usual) access in obj 4
	uint16_t unknown; // 0x0000
} ee_setpoint;

uint32_t iap_ee_read(uint32_t eeaddr, void* dst, uint32_t len);
uint32_t iap_ee_write(uint32_t eeaddr, void* src, uint32_t len);

#endif /* EE_H_ */
