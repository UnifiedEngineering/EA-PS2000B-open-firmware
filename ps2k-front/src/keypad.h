#ifndef KEYPAD_H_
#define KEYPAD_H_

typedef enum {
	ENC_CH1_VOLT = 0,
	ENC_CH1_CURR,
	ENC_CH2_VOLT,
	ENC_CH2_CURR,
	ENC_NUM_ENCODERS
} encoder_t;

typedef enum {
	KEY_CH1_PRESET = _BV(0),
	KEY_CH1_ON_OFF = _BV(1),
	KEY_CH1_VOLT = _BV(2),
	KEY_CH1_CURR = _BV(3),
	KEY_CH2_PRESET = _BV(4),
	KEY_CH2_ON_OFF = _BV(5),
	KEY_CH2_VOLT = _BV(6),
	KEY_CH2_CURR = _BV(7),
	KEY_TRACKING = _BV(8),
} key_t;

void keypad_init(void);
int32_t get_encoder_delta(encoder_t encodernum);
key_t get_keys_pressed(void);
key_t get_keys_down(void);

#endif /* KEYPAD_H_ */
