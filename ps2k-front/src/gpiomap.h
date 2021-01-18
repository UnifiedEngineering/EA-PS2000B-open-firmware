#ifndef GPIOMAP_H_
#define GPIOMAP_H_

#include "chip.h"

/*
uart0 connects to ch1 @500kbps
uart1 connects to ch2 @500kbps

37=p0.0=ch1 preset btn (S1)
38=p0.1=ch1 volt enc a
64=p0.6=always low (connected to pin 19 of hc244#2, 2OE)
63=p0.7=always low (connected to pin1 of hc244#2, 1OE)
62=p0.8=ch1 on/off btn (S2)
61=p0.9=tracking btn (S9)
39=p0.10=ch1 volt enc b
40=p0.11=ch1 volt enc pushbtn (S3)
46=p0.17=ch1 cur enc a
45=p0.18=ch1 curr encoder pushbtn (S4)
44=p0.22=ch1 cur enc b

75=p1.1=display cs for ch1 (out) connected to pin 19 of hc244#1, 2oe
74=p1.4=display rd maybe (out). keep high
73=p1.8=display clk(wr) shared (out)
72=p1.9=display data shared (out)
71=p1.10=display cs for ch2 (out) to hc244#1 pin1, 1oe
70=p1.14=ch1 reset connected to pin9 of ch1 conn through hc244#2 (out)
69=p1.15=ch1 isp mode connected to pin10 of ch1 conn through hc244#2 (out)
36=p1.29=tracking LED drive to cathode (low for on) (out)

60=p2.0=ch2 on/off btn (S6)
59=p2.1=ch2 cur enc a
58=p2.2=ch2 cur enc b
55=p2.3=ch2 curr encoder pushbtn (S5)
54=p2.4=ch2 volt enc a
53=p2.5=ch2 volt enc b
52=p2.6=ch2 volt enc pushbtn (S8)
51=p2.7=ch2 preset btn (S7)

65=p4.28=ch2 reset to pin 6 on hc244#2 1a3 (then to ch2 con pin 9)
68=p4.29=ch2 isp mode to pin 8 on hc244#2 1a4 (then to ch2 con pin 10)
*/

#define _BV(x) (1 << x)

// Port0 pins
#define OE1_PORT (0)
#define OE1_BIT (6)

#define OE2_PORT (0)
#define OE2_BIT (7)

#define BTN_CH1_PRESET_PORT (0)
#define BTN_CH1_PRESET_BIT (0)

#define ENC_A_CH1_VOLT_PORT (0)
#define ENC_A_CH1_VOLT_BIT (1)

#define BTN_CH1_ON_OFF_PORT (0)
#define BTN_CH1_ON_OFF_BIT (8)

#define BTN_TRACKING_PORT (0)
#define BTN_TRACKING_BIT (9)

#define ENC_B_CH1_VOLT_PORT (0)
#define ENC_B_CH1_VOLT_BIT (10)

#define BTN_CH1_VOLT_PORT (0)
#define BTN_CH1_VOLT_BIT (11)

#define ENC_A_CH1_CURR_PORT (0)
#define ENC_A_CH1_CURR_BIT (17)

#define BTN_CH1_CURR_PORT (0)
#define BTN_CH1_CURR_BIT (18)

#define ENC_B_CH1_CURR_PORT (0)
#define ENC_B_CH1_CURR_BIT (22)

#define PORT0_DIR (_BV(OE1_BIT) | _BV(OE2_BIT))


// Port1 pins
#define CH1_RESET_PORT (1)
#define CH1_RESET_BIT (14)

#define CH1_ISP_PORT (1)
#define CH1_ISP_BIT (15)

#define LED_TRACKING_PORT (1)
#define LED_TRACKING_BIT (29)
#define LED_TRACKING(x) do {if (x) LPC_GPIO[LED_TRACKING_PORT].CLR = _BV(LED_TRACKING_BIT); else LPC_GPIO[LED_TRACKING_PORT].SET = _BV(LED_TRACKING_BIT);} while (0);

#define SPI_RD_PORT (1)
#define SPI_RD_BIT (4)

#define SPI_CS0_PORT (1)
#define SPI_CS0_BIT (1)

#define SPI_CS1_PORT (1)
#define SPI_CS1_BIT (10)

#define SPI_DAT_PORT (1)
#define SPI_DAT_BIT (9)

#define SPI_CLK_PORT (1)
#define SPI_CLK_BIT (8)

#define SPI_RD(x) do {if (x) LPC_GPIO[SPI_RD_PORT].SET = _BV(SPI_RD_BIT); else LPC_GPIO[SPI_RD_PORT].CLR = _BV(SPI_RD_BIT);} while (0);
#define SPI_CS0(x) do {if (x) LPC_GPIO[SPI_CS0_PORT].SET = _BV(SPI_CS0_BIT); else LPC_GPIO[SPI_CS0_PORT].CLR = _BV(SPI_CS0_BIT);} while (0);
#define SPI_CS1(x) do {if (x) LPC_GPIO[SPI_CS1_PORT].SET = _BV(SPI_CS1_BIT); else LPC_GPIO[SPI_CS1_PORT].CLR = _BV(SPI_CS1_BIT);} while (0);
#define SPI_DAT(x) do {if (x) LPC_GPIO[SPI_DAT_PORT].SET = _BV(SPI_DAT_BIT); else LPC_GPIO[SPI_DAT_PORT].CLR = _BV(SPI_DAT_BIT);} while (0);
#define SPI_CLK(x) do {if (x) LPC_GPIO[SPI_CLK_PORT].SET = _BV(SPI_CLK_BIT); else LPC_GPIO[SPI_CLK_PORT].CLR = _BV(SPI_CLK_BIT);} while (0);

#define PORT1_DIR (_BV(CH1_RESET_BIT) | _BV(CH1_ISP_BIT) | _BV(LED_TRACKING_BIT) | \
		_BV(SPI_RD_BIT | _BV(SPI_CS0_BIT) | _BV(SPI_CS1_BIT) | _BV(SPI_DAT_BIT) | _BV(SPI_CLK_BIT)))


// Port2 pins
#define BTN_CH2_ON_OFF_PORT (2)
#define BTN_CH2_ON_OFF_BIT (0)

#define ENC_A_CH2_CURR_PORT (2)
#define ENC_A_CH2_CURR_BIT (1)

#define ENC_B_CH2_CURR_PORT (2)
#define ENC_B_CH2_CURR_BIT (2)

#define BTN_CH2_CURR_PORT (2)
#define BTN_CH2_CURR_BIT (3)

#define ENC_A_CH2_VOLT_PORT (2)
#define ENC_A_CH2_VOLT_BIT (4)

#define ENC_B_CH2_VOLT_PORT (2)
#define ENC_B_CH2_VOLT_BIT (5)

#define BTN_CH2_VOLT_PORT (2)
#define BTN_CH2_VOLT_BIT (6)

#define BTN_CH2_PRESET_PORT (2)
#define BTN_CH2_PRESET_BIT (7)

#define PORT2_DIR (0)


// Port4 pins
#define CH2_RESET_PORT (4)
#define CH2_RESET_BIT (28)

#define CH2_ISP_PORT (4)
#define CH2_ISP_BIT (29)

#define PORT4_DIR (_BV(CH2_RESET_BIT) | _BV(CH2_ISP_BIT))

#define NUM_CHANNELS (2)

// Ch0 or 1 for simplicity during indexing
#define CHx_RESET_PORT(x) (x ? CH2_RESET_PORT : CH1_RESET_PORT)
#define CHx_RESET_BIT(x) (x ? CH2_RESET_BIT : CH1_RESET_BIT)
#define CHx_RESET(x, y) do {if (y) LPC_GPIO[CHx_RESET_PORT(x)].SET = _BV(CHx_RESET_BIT(x)); else LPC_GPIO[CHx_RESET_PORT(x)].CLR = _BV(CHx_RESET_BIT(x));} while (0);

#define CHx_ISP_PORT(x) (x ? CH2_ISP_PORT : CH1_ISP_PORT)
#define CHx_ISP_BIT(x) (x ? CH2_ISP_BIT : CH1_ISP_BIT)
#define CHx_ISP(x, y) do {if (y) LPC_GPIO[CHx_ISP_PORT(x)].SET = _BV(CHx_ISP_BIT(x)); else LPC_GPIO[CHx_ISP_PORT(x)].CLR = _BV(CHx_ISP_BIT(x));} while (0);

#define CHx_UART(x) (x ? LPC_UART1 : LPC_UART0)

#endif /* GPIOMAP_H_ */
