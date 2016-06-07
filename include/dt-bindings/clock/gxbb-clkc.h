/*
 * Meson8b clock tree IDs
 */

#ifndef __GXBB_CLKC_H
#define __GXBB_CLKC_H

#if 0
#define CLKID_UNUSED		0
#define CLKID_XTAL		1
#define CLKID_PLL_VID		3
#define CLKID_PLL_SYS		4
#define CLKID_CLK81		10
#define CLKID_MALI		11
#define CLKID_CPUCLK		12
#define CLKID_ZERO		13
#define CLKID_MPEG_SEL		14
#define CLKID_MPEG_DIV		15
#endif

#define CLKID_SYS_PLL		0
#define CLKID_CPUCLK		1
#define CLKID_FIXED_PLL		2
#define CLKID_FCLK_DIV2		3
#define CLKID_FCLK_DIV3		4
#define CLKID_FCLK_DIV4		5
#define CLKID_FCLK_DIV5		6
#define CLKID_FCLK_DIV7		7
#define CLKID_HDMI_PLL		8

#define CLK_NR_CLKS		9

#endif /* __MESON8B_CLKC_H */
