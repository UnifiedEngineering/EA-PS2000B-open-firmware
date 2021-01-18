#include "chip.h"

const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

STATIC const PINMUX_GRP_T pinmuxing[] = {
	{0, 2,  IOCON_MODE_INACT | IOCON_FUNC1}, // TXD0
	{0, 3,  IOCON_MODE_INACT | IOCON_FUNC1}, // RXD0
	{0, 15, IOCON_MODE_INACT | IOCON_FUNC1}, // TXD1
	{0, 16, IOCON_MODE_INACT | IOCON_FUNC1}, // RXD1

	{1, 30, IOCON_MODE_INACT | IOCON_FUNC2}, // USB VBUS
	{0, 29, IOCON_MODE_INACT | IOCON_FUNC1}, // USB D+
	{0, 30, IOCON_MODE_INACT | IOCON_FUNC1}, // USB D-
	{2, 9,  IOCON_MODE_INACT | IOCON_FUNC1}, // USB soft-connect
};

void SystemInit(void)
{
// This has already have been setup by bootloader
#ifdef SETUP_VTOR
	unsigned int *pSCB_VTOR = (unsigned int *) 0xE000ED08;
	extern void *g_pfnVectors;
	*pSCB_VTOR = (unsigned int) &g_pfnVectors;
#endif

	Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
	Chip_SetupXtalClocking();
	Chip_SYSCTL_SetFLASHAccess(FLASHTIM_100MHZ_CPU);
}
