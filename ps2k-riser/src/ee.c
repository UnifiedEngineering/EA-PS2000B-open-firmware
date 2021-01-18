/*
 * ee.c - EEPROM access using IAP ROM calls
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
#include "ee.h"

uint32_t iap_ee_read(uint32_t eeaddr, void* dst, uint32_t len) {
	EEPROM_READ_COMMAND_T command = {.cmd = FLASH_EEPROM_READ,
									.eepromAddr = eeaddr,
									.ramAddr = (uint32_t)dst,
									.byteNum = len,
									.cclk = 72000};
	EEPROM_READ_OUTPUT_T result;
	Chip_EEPROM_Read(&command, &result);
	return result.status;
}

uint32_t iap_ee_write(uint32_t eeaddr, void* src, uint32_t len) {
	EEPROM_WRITE_COMMAND_T command = {.cmd = FLASH_EEPROM_WRITE,
									.eepromAddr = eeaddr,
									.ramAddr = (uint32_t)src,
									.byteNum = len,
									.cclk = 72000};
	EEPROM_WRITE_OUTPUT_T result;
	Chip_EEPROM_Write(&command, &result);
	return result.status;
}
