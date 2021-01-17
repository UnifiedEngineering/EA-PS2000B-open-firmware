# EA-PS2000B-open-firmware
Alternate open-source firmware for the EA Elektro-Automatik PS 2300B power supplies

## Original firmware issues
This firmware attempts to rectify a few issues mainly with the front panel operations of the PS2000B-series power supplies. The issues I encountered with the original firmware (my power supply was shipped with firmware version 3.06 dated 2018-12-27):
* The rotary encoders with their corresponding push buttons aren't always responding (at first I thought a few of the push buttons were faulty). Also no acceleration when turning knobs fast.
* Voltage setpoint can be adjusted in 1V or 0.1V steps, but the actual setpoints aren't matching up with the DAC steps sometimes leading to a setpoint 100+mV away from the one shown on the display. The setpoints are controlled using PWM outputs from the MCU which are heavily low-pass filtered. The original firmware outputs ~35kHz PWM which would correspond to ~11 bits resolution (72MHz MCU frequency / 35kHz = 2057 steps). For a 84V output supply this would in theory result in 84 / 2048 = 41mV per step. That's not the whole story though as there seems to be a 10% overrange margin (the setpoint for 84V output is around 3V, not 3.3V, so it's really 92.4V / 2048 = 45mV per step). There seems to be some additional rounding happening in the original riser firmware as stepping the output voltage in 45mV steps isn't possible.
* Voltage readback showing 84.00V (two decimals), but slowly decreasing the voltage will not update the display until it reaches 83.90V. Basically the last digit is always forced to zero leading to a readout that is almost always too low.

I asked Elektro-Automatik technical support about these issues and asked whether any more firmware would be released for these units and was told that there would be no further updates. Cue this alternate firmware. Note that it currently only supports the "triple" units with the blue/white segment displays! I have no idea how the single-output variants or the color touch models in the PS2000B series are put together at this point (and if they have the same issues). Input appreciated!

## Hardware description
The triple output PS2384-05B supply consists of two PS2000 LT MC riser boards each controlling one power supply board. Both of these riser boards are connected to a single front panel board with all buttons and both displays on. The third output consists of a tiny riser board off of the second channel power board (not possible to control from the front panel MCU).

The riser boards controls their corresponding power supply using two analog voltages (one for voltage setpoint and one for current setpoint) and receives three analog voltages (voltage and current readback plus power supply temperature). Care must be taken to limit output power to the nominal rated power (voltage and current setpoints must not both simultaneously be set to maximum for instance, dynamic power limiting will take care of that).

The PS2000 LT MC riser boards normally boots their firmware from internal flash (and my modules reports version 406, or 4.06, firmware from way back in 2014). Due to the fact that the ISP mode pin, the Reset pin and the ISP mode UART are all connected to the front panel MCU this makes ISP boot possible.

I'm using the ISP mode and loading the alternate riser firmware directly into RAM which means that restoring original firmware operation is as simple as restoring the original `firmware.bin` file to the front panel (which does *NOT* contain any riser board firmware, it assumes that the riser will boot on its own).

## Open-source implementation
Still a work-in-progress, the following front panel parts are working:
* SPI segment displays, 7-seg font and glyphs
* Keys and rotary encoder input
* Basic UART communication to the riser modules (send setpoints and receive readback)
* USB CDC ACM driver (just echoing data back to host)
* Rough initial NXP ISP support for RAM-booting the modules

Front panel work in progress/not implemented yet:
* Retrieving stored setpoints, including over-voltage/current (all stored in riser modules)
* Tracking mode
* Preset "menu", OVP/OCP, Lock
* USB communication protocol (SCPI perhaps?)

The following riser firmware parts are working:
* UART communication (only receives setpoints and send fake readback at the moment)
* PWM outputs driving voltage and current setpoints (higher resolution than original firmware)
* Debug output over SWO

Riser module in progress/not implemented yet:
* ADC readback (including thermal and OVP/OCP shutdown)
* Readout of stored setpoints
* Storing setpoints to EEPROM (after a timeout, limiting EEPROM programming cycles)

Project name | Description
-------------|------------
ps2k-front | Alternate PS2300 front panel LPC1752 firmware (includes the ps2k-riser firmware if compiled with ISP RAM-load support)
ps2k-riser | Alternate PS2000 LT MC riser LPC1315 firmware
lpc_chip_175x_6x | [LPC Open files used by ps2k-front](https://www.nxp.com/design/microcontrollers-developer-resources/lpcopen-libraries-and-examples/lpcopen-software-development-platform-lpc17xx:LPCOPEN-SOFTWARE-FOR-LPC17XX)
lpc_chip_13xx | [LPC Open files used by ps2k-riser](https://www.nxp.com/design/microcontrollers-developer-resources/lpcopen-libraries-and-examples/lpcopen-software-development-platform-lpc13xx:LPCOPEN-SOFTWARE-FOR-LPC13XX)

Built using NXP [MCUXpressoIDE](https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE) 11.2.1.

## Build steps:
* Import the four projects in a (new) MCPXpressoIDE workspace
* Build the ps2k-front firmware, this will produce the required firmware.bin file.

## Flash power supply unit:
* Connect USB cable when power supply is off
* Hold `Ch1 Preset` and `Ch1 On/Off` buttons while turning power on.
* The power supply should appear as a `PS2000T` USB mass storage device. **Make a backup of the original `firmware.bin` file!**
* Delete the `firmware.bin` from the `PS2000T` drive.
* Copy the new `firmware.bin` file to the `PS2000T` drive. I'm using the following on macOS: `rm /Volumes/PS2000T/firmware.bin && cp -X firmware.bin /Volumes/PS2000T/firmware.bin && diskutil unmount /Volumes/PS2000T`

Power cycle power supply.
Done!

As always, no warranty (but works for me on a PS2384-05B supply).
