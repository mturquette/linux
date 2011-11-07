/*
 * OMAP4-specific DPLL control functions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Rajendra Nayak
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/bitops.h>

#include <plat/cpu.h>
#include <plat/clock.h>

#include "clock.h"
#include "cm-regbits-44xx.h"
#include "cm-regbits-54xx.h"

#define OMAP_1_4GHz	1400000000

int omap5_mpu_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	struct dpll_data *dd;
	u32 v;
	unsigned long dpll_rate;

	if (!clk || !rate || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	if (!clk->parent->set_rate)
		return -EINVAL;

	/*
	 * On OMAP5430, to obtain MPU DPLL frequency higher
	 * than 1.4GHz, DCC (Duty Cycle Correction) needs to
	 * be enabled.
	 * And needs to be kept disabled for <= 1.4 Ghz.
	 */
	dpll_rate = omap2_get_dpll_rate(clk->parent);
	v = __raw_readl(dd->mult_div1_reg);
	if (rate <= OMAP_1_4GHz) {
		if (rate == dpll_rate)
			return 0;
		/* If DCC is enabled, disable it */
		if (v & OMAP5430_DCC_EN_MASK) {
			v &= ~OMAP5430_DCC_EN_MASK;
			__raw_writel(v, dd->mult_div1_reg);
		}
		clk->parent->set_rate(clk->parent, rate);
	} else {
		if (rate == dpll_rate/2)
			return 0;
		v |= OMAP5430_DCC_EN_MASK;
		__raw_writel(v, dd->mult_div1_reg);
		/*
		 * On OMAP54530, the MPU clk for frequencies higher than 1.4Ghz
		 * is sourced from CLKOUTX2_M3, instead of CLKOUT_M2, while
		 * value of M3 is fixed to 1. Hence for frequencies higher
		 * than 1 Ghz, lock the DPLL at half the rate so the
		 * CLKOUTX2_M3 then matches the requested rate.
		 */
		clk->parent->set_rate(clk->parent, rate/2);
	}

	clk->rate = rate;

	return 0;
}

long omap5_mpu_dpll_round_rate(struct clk *clk, unsigned long rate)
{
	if (!clk || !rate || !clk->parent)
		return -EINVAL;

	if (clk->parent->round_rate)
		return clk->parent->round_rate(clk->parent, rate);
	else
		return 0;
}

unsigned long omap5_mpu_dpll_recalc(struct clk *clk)
{
	struct dpll_data *dd;
	u32 v;

	if (!clk || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	v = __raw_readl(dd->mult_div1_reg);
	if (v & OMAP5430_DCC_EN_MASK)
		return omap2_get_dpll_rate(clk->parent) * 2;
	else
		return omap2_get_dpll_rate(clk->parent);
}

