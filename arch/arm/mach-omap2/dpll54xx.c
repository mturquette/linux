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
#include <plat/common.h>
#include <mach/omap4-common.h>

#include "clock.h"
#include "clock54xx.h"
#include "cm.h"
#include "cm1_54xx.h"
#include "clockdomain.h"
#include "cm-regbits-54xx.h"

#define MAX_FREQ_UPDATE_TIMEOUT  100000
#define OMAP_1_4GHz	1400000000

static struct clockdomain *l3_emif_clkdm;

/**
 * omap4_core_dpll_m2_set_rate - set CORE DPLL M2 divider
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Programs the CM shadow registers to update CORE DPLL M2
 * divider. M2 divider is used to clock external DDR and its
 * reconfiguration on frequency change is managed through a
 * hardware sequencer. This is managed by the PRCM with EMIF
 * uding shadow registers.
 * Returns -EINVAL/-1 on error and 0 on success.
 */
int omap5_core_dpll_m2_set_rate(struct clk *clk, unsigned long rate)
{
	int i = 0;
	u32 validrate = 0, shadow_freq_cfg1 = 0, new_div = 0;

	if (!clk || !rate)
		return -EINVAL;

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm) {
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
		if (!l3_emif_clkdm) {
			pr_err("%s: clockdomain lookup failed\n", __func__);
			return -EINVAL;
		}
	}

	/* Configures MEMIF domain in SW_WKUP */
	clkdm_wakeup(l3_emif_clkdm);

	/*
	 * XXX TODO: Program EMIF timing parameters in EMIF shadow registers
	 * for targetted DRR clock.
	 * DDR Clock = core_dpll_m2 / 2
	 */
	/* omap_emif_setup_registers(validrate >> 1, LPDDR2_VOLTAGE_STABLE); */

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - DLL_RESET=1 (DLL must be reset upon frequency change)
	 * - DPLL_CORE_M2_DIV with same value as the one already
	 *	in direct register
	 * - DPLL_CORE_DPLL_EN=0x7 (to make CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = (1 << OMAP54XX_DLL_RESET_SHIFT) |
			(new_div << OMAP54XX_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOCKED << OMAP54XX_DPLL_CORE_DPLL_EN_SHIFT) |
			(1 << OMAP54XX_FREQ_UPDATE_SHIFT);
	shadow_freq_cfg1 &= ~OMAP54XX_DLL_OVERRIDE_MASK;
	__raw_writel(shadow_freq_cfg1, OMAP54XX_CM_SHADOW_FREQ_CONFIG1_OFFSET);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP54XX_CM_SHADOW_FREQ_CONFIG1_OFFSET)
				& OMAP54XX_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Configures MEMIF domain back to HW_WKUP */
	clkdm_allow_idle(l3_emif_clkdm);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		return -1;
	}

	/* Update the clock change */
	clk->rate = validrate;

	return 0;
}

/**
 * omap5_core_dpll_m5_set_rate - set CORE DPLL M5 divider
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Programs the CM shadow registers to update CORE DPLL M5
 * divider. M5 divider is used to clock l3 and GPMC. GPMC
 * reconfiguration on frequency change is managed through a
 * hardware sequencer using shadow registers.
 * Returns -EINVAL/-1 on error and 0 on success.
 */
int omap5_core_dpll_h12_set_rate(struct clk *clk, unsigned long rate)
{
	int i = 0;
	u32 validrate = 0, shadow_freq_cfg2 = 0, shadow_freq_cfg1, new_div = 0;
	int ret = 0;
	if (!clk || !rate)
		return -EINVAL;

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/*
	 * FREQ_UPDATE sequence:
	 * - DPLL_CORE_M5_DIV with new value of M5 post-divider on L3 clock generation path
	 * - CLKSEL_L3=1 (unchanged)
	 * - CLKSEL_CORE=0 (unchanged)
	 * - GPMC_FREQ_UPDATE=1
	 */

	shadow_freq_cfg2 = (new_div << OMAP54XX_DPLL_CORE_H12_DIV_SHIFT) |
						(1 << OMAP54XX_CLKSEL_L3_1_1_SHIFT) |
						(1 << OMAP54XX_FREQ_UPDATE_SHIFT);

	__raw_writel(shadow_freq_cfg2, OMAP54XX_CM_SHADOW_FREQ_CONFIG2_OFFSET);

	/* Write to FREQ_UPDATE of SHADOW_FREQ_CONFIG1 to trigger transition */
	shadow_freq_cfg1 = __raw_readl(OMAP54XX_CM_SHADOW_FREQ_CONFIG1_OFFSET);
	shadow_freq_cfg1 |= (1 << OMAP54XX_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg1, OMAP54XX_CM_SHADOW_FREQ_CONFIG1_OFFSET);

	/* wait for the configuration to be applied by Polling FREQ_UPDATE of SHADOW_FREQ_CONFIG1 */
	omap_test_timeout(((__raw_readl(OMAP54XX_CM_SHADOW_FREQ_CONFIG1_OFFSET)
				& OMAP54XX_GPMC_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		ret = -1;
		goto out;
	}

	/* Update the clock change */
	clk->rate = validrate;

out:
	/* Disable GPMC FREQ_UPDATE */
	shadow_freq_cfg2 &= ~(1 << OMAP54XX_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg2, OMAP54XX_CM_SHADOW_FREQ_CONFIG2_OFFSET);

	return ret;
}


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
		if (v & OMAP54XX_DCC_EN_MASK) {
			v &= ~OMAP54XX_DCC_EN_MASK;
			__raw_writel(v, dd->mult_div1_reg);
		}
		clk->parent->set_rate(clk->parent, rate);
	} else {
		if (rate == dpll_rate/2)
			return 0;
		v |= OMAP54XX_DCC_EN_MASK;
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
	if (v & OMAP54XX_DCC_EN_MASK)
		return omap2_get_dpll_rate(clk->parent) * 2;
	else
		return omap2_get_dpll_rate(clk->parent);
}

