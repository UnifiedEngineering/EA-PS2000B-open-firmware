#ifndef ISPUTILS_H_
#define ISPUTILS_H_

#include "chip.h"

void isp_mode(void);
uint32_t calc_checksum(uint8_t* buf, uint32_t size);

#endif /* ISPUTILS_H_ */
