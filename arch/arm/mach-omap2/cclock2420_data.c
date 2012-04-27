/*
 * OMAP2420 clock data
 *
 * Copyright (C) 2005-2009 Texas Instruments, Inc.
 * Copyright (C) 2004-2011 Nokia Corporation
 *
 * Contacts:
 * Richard Woodruff <r-woodruff2@ti.com>
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/list.h>

#include <plat/hardware.h>
#include <plat/clkdev_omap.h>

#include "iomap.h"
#include "clock.h"
#include "clock2xxx.h"
#include "opp2xxx.h"
#include "cm2xxx_3xxx.h"
#include "prm2xxx_3xxx.h"
#include "prm-regbits-24xx.h"
#include "cm-regbits-24xx.h"
#include "sdrc.h"
#include "control.h"

#define OMAP_CM_REGADDR                 OMAP2420_CM_REGADDR

/*
 * 2420 clock tree.
 *
 * NOTE:In many cases here we are assigning a 'default' parent. In
 *	many cases the parent is selectable. The set parent calls will
 *	also switch sources.
 *
 *	Several sources are given initial rates which may be wrong, this will
 *	be fixed up in the init func.
 *
 *	Things are broadly separated below by clock domains. It is
 *	noteworthy that most peripherals have dependencies on multiple clock
 *	domains. Many get their interface clocks from the L4 domain, but get
 *	functional clocks from fixed sources or other core domain derived
 *	clocks.
 */

DEFINE_CLK_FIXED_RATE(alt_ck, CLK_IS_ROOT, 54000000, 0x0);

DEFINE_CLK_FIXED_RATE(func_32k_ck, CLK_IS_ROOT, 32768, 0x0);

DEFINE_CLK_FIXED_RATE(mcbsp_clks, CLK_IS_ROOT, 0x0, 0x0);

DEFINE_CLK_FIXED_RATE(osc_ck, CLK_IS_ROOT, 0x0, 0x0);

DEFINE_CLK_FIXED_RATE(secure_32k_ck, CLK_IS_ROOT, 32768, 0x0);

static struct clk sys_ck;

static const char *sys_ck_parent_names[] = {
	"osc_ck",
};

static const struct clk_ops sys_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2xxx_sys_clk_recalc,
};

static struct clk_hw_omap sys_ck_hw = {
	.hw = {
		.clk = &sys_ck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sys_ck = {
	.name		= "sys_ck",
	.hw		= &sys_ck_hw.hw,
	.parent_names	= sys_ck_parent_names,
	.num_parents	= ARRAY_SIZE(sys_ck_parent_names),
	.ops		= &sys_ck_ops,
};

static struct dpll_data dpll_dd = {
	.mult_div1_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.mult_mask	= OMAP24XX_DPLL_MULT_MASK,
	.div1_mask	= OMAP24XX_DPLL_DIV_MASK,
	.clk_bypass	= &sys_ck,
	.clk_ref	= &sys_ck,
	.control_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_mask	= OMAP24XX_EN_DPLL_MASK,
	.max_multiplier	= 1023,
	.min_divider	= 1,
	.max_divider	= 16,
};

static struct clk dpll_ck;

static const char *dpll_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
	.recalc_rate	= &omap2_dpllcore_recalc,
	.set_rate	= &omap2_reprogram_dpllcore,
};

static struct clk_hw_omap dpll_ck_hw = {
	.hw = {
		.clk = &dpll_ck,
	},
	.dpll_data	= &dpll_dd,
	.clkdm_name	= "wkup_clkdm",
	.allow_idle	= &_allow_idle,
	.deny_idle	= &_deny_idle,
};

static struct clk dpll_ck = {
	.name		= "dpll_ck",
	.hw		= &dpll_ck_hw.hw,
	.parent_names	= dpll_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll_ck_parent_names),
	.ops		= &dpll_ck_ops,
};

static struct clk core_ck;

static const char *core_ck_parent_names[] = {
	"dpll_ck",
};

static const struct clk_ops core_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_ck_hw = {
	.hw = {
		.clk = &core_ck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk core_ck = {
	.name		= "core_ck",
	.hw		= &core_ck_hw.hw,
	.parent_names	= core_ck_parent_names,
	.num_parents	= ARRAY_SIZE(core_ck_parent_names),
	.ops		= &core_ck_ops,
};

static const struct clksel_rate core_l3_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_242X },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 16, .val = 16, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel core_l3_clksel[] = {
	{ .parent = &core_ck, .rates = core_l3_core_rates },
	{ .parent = NULL },
};

static const char *core_l3_ck_parent_names[] = {
	"core_ck",
};

static struct clk core_l3_ck;

static const struct clk_ops core_l3_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap core_l3_ck_hw = {
	.hw = {
		.clk = &core_l3_ck,
	},
	.clksel		= core_l3_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_L3_MASK,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk core_l3_ck = {
	.name		= "core_l3_ck",
	.hw		= &core_l3_ck_hw.hw,
	.parent_names	= core_l3_ck_parent_names,
	.num_parents	= ARRAY_SIZE(core_l3_ck_parent_names),
	.ops		= &core_l3_ck_ops,
};

static const struct clksel_rate l4_core_l3_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel l4_clksel[] = {
	{ .parent = &core_l3_ck, .rates = l4_core_l3_rates },
	{ .parent = NULL },
};

static const char *l4_ck_parent_names[] = {
	"core_l3_ck",
};

static struct clk l4_ck;

static const struct clk_ops l4_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap l4_ck_hw = {
	.hw = {
		.clk = &l4_ck,
	},
	.clksel		= l4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_L4_MASK,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk l4_ck = {
	.name		= "l4_ck",
	.hw		= &l4_ck_hw.hw,
	.parent_names	= l4_ck_parent_names,
	.num_parents	= ARRAY_SIZE(l4_ck_parent_names),
	.ops		= &l4_ck_ops,
};

static struct clk aes_ick;

static const char *aes_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops aes_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap aes_ick_hw = {
	.hw = {
		.clk = &aes_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_AES_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk aes_ick = {
	.name		= "aes_ick",
	.hw		= &aes_ick_hw.hw,
	.parent_names	= aes_ick_parent_names,
	.num_parents	= ARRAY_SIZE(aes_ick_parent_names),
	.ops		= &aes_ick_ops,
};

static struct clk apll54_ck;

static const char *apll54_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops apll54_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_clk_apll54_enable,
	.disable	= &omap2_clk_apll_disable,
};

static struct clk_hw_omap apll54_ck_hw = {
	.hw = {
		.clk = &apll54_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP24XX_EN_54M_PLL_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
	.allow_idle	= &_apll54_allow_idle,
	.deny_idle	= &_apll54_deny_idle,
};

static struct clk apll54_ck = {
	.name		= "apll54_ck",
	.hw		= &apll54_ck_hw.hw,
	.parent_names	= apll54_ck_parent_names,
	.num_parents	= ARRAY_SIZE(apll54_ck_parent_names),
	.ops		= &apll54_ck_ops,
};

static struct clk apll96_ck;

static const char *apll96_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops apll96_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_clk_apll96_enable,
	.disable	= &omap2_clk_apll_disable,
};

static struct clk_hw_omap apll96_ck_hw = {
	.hw = {
		.clk = &apll96_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP24XX_EN_96M_PLL_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
	.allow_idle	= &_apll96_allow_idle,
	.deny_idle	= &_apll96_deny_idle,
};

static struct clk apll96_ck = {
	.name		= "apll96_ck",
	.hw		= &apll96_ck_hw.hw,
	.parent_names	= apll96_ck_parent_names,
	.num_parents	= ARRAY_SIZE(apll96_ck_parent_names),
	.ops		= &apll96_ck_ops,
};

static struct clk func_96m_ck;

static const char *func_96m_ck_parent_names[] = {
	"apll96_ck",
};

static const struct clk_ops func_96m_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap func_96m_ck_hw = {
	.hw = {
		.clk = &func_96m_ck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk func_96m_ck = {
	.name		= "func_96m_ck",
	.hw		= &func_96m_ck_hw.hw,
	.parent_names	= func_96m_ck_parent_names,
	.num_parents	= ARRAY_SIZE(func_96m_ck_parent_names),
	.ops		= &func_96m_ck_ops,
};

static struct clk cam_fck;

static const char *cam_fck_parent_names[] = {
	"func_96m_ck",
};

static const struct clk_ops cam_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap cam_fck_hw = {
	.hw = {
		.clk = &cam_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_CAM_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk cam_fck = {
	.name		= "cam_fck",
	.hw		= &cam_fck_hw.hw,
	.parent_names	= cam_fck_parent_names,
	.num_parents	= ARRAY_SIZE(cam_fck_parent_names),
	.ops		= &cam_fck_ops,
};

static struct clk cam_ick;

static const char *cam_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops cam_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap cam_ick_hw = {
	.hw = {
		.clk = &cam_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_CAM_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk cam_ick = {
	.name		= "cam_ick",
	.hw		= &cam_ick_hw.hw,
	.parent_names	= cam_ick_parent_names,
	.num_parents	= ARRAY_SIZE(cam_ick_parent_names),
	.ops		= &cam_ick_ops,
};

static struct clk des_ick;

static const char *des_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops des_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap des_ick_hw = {
	.hw = {
		.clk = &des_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_DES_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk des_ick = {
	.name		= "des_ick",
	.hw		= &des_ick_hw.hw,
	.parent_names	= des_ick_parent_names,
	.num_parents	= ARRAY_SIZE(des_ick_parent_names),
	.ops		= &des_ick_ops,
};

static const struct clksel_rate dsp_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel dsp_fck_clksel[] = {
	{ .parent = &core_ck, .rates = dsp_fck_core_rates },
	{ .parent = NULL },
};

static const char *dsp_fck_parent_names[] = {
	"core_ck",
};

static struct clk dsp_fck;

static const struct clk_ops dsp_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dsp_fck_hw = {
	.hw = {
		.clk = &dsp_fck,
	},
	.clksel		= dsp_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_DSP_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_CM_FCLKEN_DSP_EN_DSP_SHIFT,
	.clkdm_name	= "dsp_clkdm",
};

static struct clk dsp_fck = {
	.name		= "dsp_fck",
	.hw		= &dsp_fck_hw.hw,
	.parent_names	= dsp_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dsp_fck_parent_names),
	.ops		= &dsp_fck_ops,
};

static const struct clksel dsp_ick_clksel[] = {
	{ .parent = &dsp_fck, .rates = dsp_ick_rates },
	{ .parent = NULL },
};

static const char *dsp_ick_parent_names[] = {
	"dsp_fck",
};

static struct clk dsp_ick;

static const struct clk_ops dsp_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dsp_ick_hw = {
	.hw = {
		.clk = &dsp_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.clksel		= dsp_ick_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_DSP_IF_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP2420_EN_DSP_IPI_SHIFT,
	.clkdm_name	= "dsp_clkdm",
};

static struct clk dsp_ick = {
	.name		= "dsp_ick",
	.hw		= &dsp_ick_hw.hw,
	.parent_names	= dsp_ick_parent_names,
	.num_parents	= ARRAY_SIZE(dsp_ick_parent_names),
	.ops		= &dsp_ick_ops,
};

static const struct clksel_rate dss1_fck_sys_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate dss1_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 5, .val = 5, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 8, .flags = RATE_IN_24XX },
	{ .div = 9, .val = 9, .flags = RATE_IN_24XX },
	{ .div = 12, .val = 12, .flags = RATE_IN_24XX },
	{ .div = 16, .val = 16, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel dss1_fck_clksel[] = {
	{ .parent = &sys_ck, .rates = dss1_fck_sys_rates },
	{ .parent = &core_ck, .rates = dss1_fck_core_rates },
	{ .parent = NULL },
};

static const char *dss1_fck_parent_names[] = {
	"sys_ck",
	"core_ck",
};

static struct clk dss1_fck;

static const struct clk_ops dss1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap dss1_fck_hw = {
	.hw = {
		.clk = &dss1_fck,
	},
	.clksel		= dss1_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_DSS1_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS1_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss1_fck = {
	.name		= "dss1_fck",
	.hw		= &dss1_fck_hw.hw,
	.parent_names	= dss1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss1_fck_parent_names),
	.ops		= &dss1_fck_ops,
};

static const struct clksel_rate dss2_fck_sys_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate dss2_fck_48m_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate func_48m_apll96_rates[] = {
	{ .div = 2, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate func_48m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel func_48m_clksel[] = {
	{ .parent = &apll96_ck, .rates = func_48m_apll96_rates },
	{ .parent = &alt_ck, .rates = func_48m_alt_rates },
	{ .parent = NULL },
};

static const char *func_48m_ck_parent_names[] = {
	"apll96_ck",
	"alt_ck",
};

static struct clk func_48m_ck;

static const struct clk_ops func_48m_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap func_48m_ck_hw = {
	.hw = {
		.clk = &func_48m_ck,
	},
	.clksel		= func_48m_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_48M_SOURCE_MASK,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk func_48m_ck = {
	.name		= "func_48m_ck",
	.hw		= &func_48m_ck_hw.hw,
	.parent_names	= func_48m_ck_parent_names,
	.num_parents	= ARRAY_SIZE(func_48m_ck_parent_names),
	.ops		= &func_48m_ck_ops,
};

static const struct clksel dss2_fck_clksel[] = {
	{ .parent = &sys_ck, .rates = dss2_fck_sys_rates },
	{ .parent = &func_48m_ck, .rates = dss2_fck_48m_rates },
	{ .parent = NULL },
};

static const char *dss2_fck_parent_names[] = {
	"sys_ck",
	"func_48m_ck",
};

static struct clk dss2_fck;

static const struct clk_ops dss2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap dss2_fck_hw = {
	.hw = {
		.clk = &dss2_fck,
	},
	.clksel		= dss2_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_DSS2_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS2_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss2_fck = {
	.name		= "dss2_fck",
	.hw		= &dss2_fck_hw.hw,
	.parent_names	= dss2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss2_fck_parent_names),
	.ops		= &dss2_fck_ops,
};

static const struct clksel_rate func_54m_apll54_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate func_54m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel func_54m_clksel[] = {
	{ .parent = &apll54_ck, .rates = func_54m_apll54_rates },
	{ .parent = &alt_ck, .rates = func_54m_alt_rates },
	{ .parent = NULL },
};

static const char *func_54m_ck_parent_names[] = {
	"apll54_ck",
	"alt_ck",
};

static struct clk func_54m_ck;

static const struct clk_ops func_54m_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap func_54m_ck_hw = {
	.hw = {
		.clk = &func_54m_ck,
	},
	.clksel		= func_54m_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_54M_SOURCE_MASK,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk func_54m_ck = {
	.name		= "func_54m_ck",
	.hw		= &func_54m_ck_hw.hw,
	.parent_names	= func_54m_ck_parent_names,
	.num_parents	= ARRAY_SIZE(func_54m_ck_parent_names),
	.ops		= &func_54m_ck_ops,
};

static struct clk dss_54m_fck;

static const char *dss_54m_fck_parent_names[] = {
	"func_54m_ck",
};

static const struct clk_ops dss_54m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss_54m_fck_hw = {
	.hw = {
		.clk = &dss_54m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_TV_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_54m_fck = {
	.name		= "dss_54m_fck",
	.hw		= &dss_54m_fck_hw.hw,
	.parent_names	= dss_54m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss_54m_fck_parent_names),
	.ops		= &dss_54m_fck_ops,
};

static struct clk dss_ick;

static const char *dss_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops dss_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss_ick_hw = {
	.hw = {
		.clk = &dss_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS1_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_ick = {
	.name		= "dss_ick",
	.hw		= &dss_ick_hw.hw,
	.parent_names	= dss_ick_parent_names,
	.num_parents	= ARRAY_SIZE(dss_ick_parent_names),
	.ops		= &dss_ick_ops,
};

static struct clk eac_fck;

static const char *eac_fck_parent_names[] = {
	"func_96m_ck",
};

static const struct clk_ops eac_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap eac_fck_hw = {
	.hw = {
		.clk = &eac_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_EAC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk eac_fck = {
	.name		= "eac_fck",
	.hw		= &eac_fck_hw.hw,
	.parent_names	= eac_fck_parent_names,
	.num_parents	= ARRAY_SIZE(eac_fck_parent_names),
	.ops		= &eac_fck_ops,
};

static struct clk eac_ick;

static const char *eac_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops eac_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap eac_ick_hw = {
	.hw = {
		.clk = &eac_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_EAC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk eac_ick = {
	.name		= "eac_ick",
	.hw		= &eac_ick_hw.hw,
	.parent_names	= eac_ick_parent_names,
	.num_parents	= ARRAY_SIZE(eac_ick_parent_names),
	.ops		= &eac_ick_ops,
};

static struct clk emul_ck;

static const char *emul_ck_parent_names[] = {
	"func_54m_ck",
};

static const struct clk_ops emul_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap emul_ck_hw = {
	.hw = {
		.clk = &emul_ck,
	},
	.enable_reg	= OMAP2420_PRCM_CLKEMUL_CTRL,
	.enable_bit	= OMAP24XX_EMULATION_EN_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk emul_ck = {
	.name		= "emul_ck",
	.hw		= &emul_ck_hw.hw,
	.parent_names	= emul_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emul_ck_parent_names),
	.ops		= &emul_ck_ops,
};

static struct clk func_12m_ck;

static const char *func_12m_ck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops func_12m_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap_fixed_divisor_recalc,
};

static struct clk_hw_omap func_12m_ck_hw = {
	.hw = {
		.clk = &func_12m_ck,
	},
	.clkdm_name	= "wkup_clkdm",
	.fixed_div	= 4,
};

static struct clk func_12m_ck = {
	.name		= "func_12m_ck",
	.hw		= &func_12m_ck_hw.hw,
	.parent_names	= func_12m_ck_parent_names,
	.num_parents	= ARRAY_SIZE(func_12m_ck_parent_names),
	.ops		= &func_12m_ck_ops,
};

static struct clk fac_fck;

static const char *fac_fck_parent_names[] = {
	"func_12m_ck",
};

static const struct clk_ops fac_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap fac_fck_hw = {
	.hw = {
		.clk = &fac_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_FAC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk fac_fck = {
	.name		= "fac_fck",
	.hw		= &fac_fck_hw.hw,
	.parent_names	= fac_fck_parent_names,
	.num_parents	= ARRAY_SIZE(fac_fck_parent_names),
	.ops		= &fac_fck_ops,
};

static struct clk fac_ick;

static const char *fac_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops fac_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap fac_ick_hw = {
	.hw = {
		.clk = &fac_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_FAC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk fac_ick = {
	.name		= "fac_ick",
	.hw		= &fac_ick_hw.hw,
	.parent_names	= fac_ick_parent_names,
	.num_parents	= ARRAY_SIZE(fac_ick_parent_names),
	.ops		= &fac_ick_ops,
};

static const struct clksel gfx_fck_clksel[] = {
	{ .parent = &core_l3_ck, .rates = gfx_l3_rates },
	{ .parent = NULL },
};

static const char *gfx_2d_fck_parent_names[] = {
	"core_l3_ck",
};

static struct clk gfx_2d_fck;

static const struct clk_ops gfx_2d_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap gfx_2d_fck_hw = {
	.hw = {
		.clk = &gfx_2d_fck,
	},
	.clksel		= gfx_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP_CLKSEL_GFX_MASK,
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_2D_SHIFT,
	.clkdm_name	= "gfx_clkdm",
};

static struct clk gfx_2d_fck = {
	.name		= "gfx_2d_fck",
	.hw		= &gfx_2d_fck_hw.hw,
	.parent_names	= gfx_2d_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_2d_fck_parent_names),
	.ops		= &gfx_2d_fck_ops,
};

static struct clk gfx_3d_fck;

static const struct clk_ops gfx_3d_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap gfx_3d_fck_hw = {
	.hw = {
		.clk = &gfx_3d_fck,
	},
	.clksel		= gfx_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP_CLKSEL_GFX_MASK,
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_3D_SHIFT,
	.clkdm_name	= "gfx_clkdm",
};

static struct clk gfx_3d_fck = {
	.name		= "gfx_3d_fck",
	.hw		= &gfx_3d_fck_hw.hw,
	.parent_names	= gfx_2d_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_2d_fck_parent_names),
	.ops		= &gfx_3d_fck_ops,
};

static struct clk gfx_ick;

static const char *gfx_ick_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops gfx_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gfx_ick_hw = {
	.hw = {
		.clk = &gfx_ick,
	},
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_ICLKEN),
	.enable_bit	= OMAP_EN_GFX_SHIFT,
	.clkdm_name	= "gfx_clkdm",
};

static struct clk gfx_ick = {
	.name		= "gfx_ick",
	.hw		= &gfx_ick_hw.hw,
	.parent_names	= gfx_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_ick_parent_names),
	.ops		= &gfx_ick_ops,
};

static struct clk gpios_fck;

static const char *gpios_fck_parent_names[] = {
	"func_32k_ck",
};

static const struct clk_ops gpios_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpios_fck_hw = {
	.hw = {
		.clk = &gpios_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_GPIOS_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpios_fck = {
	.name		= "gpios_fck",
	.hw		= &gpios_fck_hw.hw,
	.parent_names	= gpios_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpios_fck_parent_names),
	.ops		= &gpios_fck_ops,
};

static struct clk wu_l4_ick;

static const char *wu_l4_ick_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops wu_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap wu_l4_ick_hw = {
	.hw = {
		.clk = &wu_l4_ick,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wu_l4_ick = {
	.name		= "wu_l4_ick",
	.hw		= &wu_l4_ick_hw.hw,
	.parent_names	= wu_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wu_l4_ick_parent_names),
	.ops		= &wu_l4_ick_ops,
};

static struct clk gpios_ick;

static const char *gpios_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops gpios_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpios_ick_hw = {
	.hw = {
		.clk = &gpios_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_GPIOS_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpios_ick = {
	.name		= "gpios_ick",
	.hw		= &gpios_ick_hw.hw,
	.parent_names	= gpios_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpios_ick_parent_names),
	.ops		= &gpios_ick_ops,
};

static struct clk gpmc_fck;

static const char *gpmc_fck_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops gpmc_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap gpmc_fck_hw = {
	.hw = {
		.clk = &gpmc_fck,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_GPMC_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk gpmc_fck = {
	.name		= "gpmc_fck",
	.hw		= &gpmc_fck_hw.hw,
	.parent_names	= gpmc_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpmc_fck_parent_names),
	.ops		= &gpmc_fck_ops,
};

static const struct clksel_rate gpt_alt_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel omap24xx_gpt_clksel[] = {
	{ .parent = &func_32k_ck, .rates = gpt_32k_rates },
	{ .parent = &sys_ck, .rates = gpt_sys_rates },
	{ .parent = &alt_ck, .rates = gpt_alt_rates },
	{ .parent = NULL },
};

static const char *gpt10_fck_parent_names[] = {
	"func_32k_ck",
	"sys_ck",
	"alt_ck",
};

static struct clk gpt10_fck;

static const struct clk_ops gpt10_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt10_fck_hw = {
	.hw = {
		.clk = &gpt10_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT10_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT10_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt10_fck = {
	.name		= "gpt10_fck",
	.hw		= &gpt10_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt10_fck_ops,
};

static struct clk gpt10_ick;

static const char *gpt10_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt10_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt10_ick_hw = {
	.hw = {
		.clk = &gpt10_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT10_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt10_ick = {
	.name		= "gpt10_ick",
	.hw		= &gpt10_ick_hw.hw,
	.parent_names	= gpt10_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_ick_parent_names),
	.ops		= &gpt10_ick_ops,
};

static struct clk gpt11_fck;

static const struct clk_ops gpt11_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt11_fck_hw = {
	.hw = {
		.clk = &gpt11_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT11_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT11_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt11_fck = {
	.name		= "gpt11_fck",
	.hw		= &gpt11_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt11_fck_ops,
};

static struct clk gpt11_ick;

static const char *gpt11_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt11_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt11_ick_hw = {
	.hw = {
		.clk = &gpt11_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT11_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt11_ick = {
	.name		= "gpt11_ick",
	.hw		= &gpt11_ick_hw.hw,
	.parent_names	= gpt11_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt11_ick_parent_names),
	.ops		= &gpt11_ick_ops,
};

static struct clk gpt12_fck;

static const struct clk_ops gpt12_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt12_fck_hw = {
	.hw = {
		.clk = &gpt12_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT12_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT12_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt12_fck = {
	.name		= "gpt12_fck",
	.hw		= &gpt12_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt12_fck_ops,
};

static struct clk gpt12_ick;

static const char *gpt12_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt12_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt12_ick_hw = {
	.hw = {
		.clk = &gpt12_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT12_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt12_ick = {
	.name		= "gpt12_ick",
	.hw		= &gpt12_ick_hw.hw,
	.parent_names	= gpt12_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt12_ick_parent_names),
	.ops		= &gpt12_ick_ops,
};

static struct clk gpt1_fck;

static const struct clk_ops gpt1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt1_fck_hw = {
	.hw = {
		.clk = &gpt1_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT1_MASK,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_GPT1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt1_fck = {
	.name		= "gpt1_fck",
	.hw		= &gpt1_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt1_fck_ops,
};

static struct clk gpt1_ick;

static const char *gpt1_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops gpt1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt1_ick_hw = {
	.hw = {
		.clk = &gpt1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_GPT1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpt1_ick = {
	.name		= "gpt1_ick",
	.hw		= &gpt1_ick_hw.hw,
	.parent_names	= gpt1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt1_ick_parent_names),
	.ops		= &gpt1_ick_ops,
};

static struct clk gpt2_fck;

static const struct clk_ops gpt2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt2_fck_hw = {
	.hw = {
		.clk = &gpt2_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT2_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt2_fck = {
	.name		= "gpt2_fck",
	.hw		= &gpt2_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt2_fck_ops,
};

static struct clk gpt2_ick;

static const char *gpt2_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt2_ick_hw = {
	.hw = {
		.clk = &gpt2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt2_ick = {
	.name		= "gpt2_ick",
	.hw		= &gpt2_ick_hw.hw,
	.parent_names	= gpt2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt2_ick_parent_names),
	.ops		= &gpt2_ick_ops,
};

static struct clk gpt3_fck;

static const struct clk_ops gpt3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt3_fck_hw = {
	.hw = {
		.clk = &gpt3_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT3_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt3_fck = {
	.name		= "gpt3_fck",
	.hw		= &gpt3_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt3_fck_ops,
};

static struct clk gpt3_ick;

static const char *gpt3_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt3_ick_hw = {
	.hw = {
		.clk = &gpt3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt3_ick = {
	.name		= "gpt3_ick",
	.hw		= &gpt3_ick_hw.hw,
	.parent_names	= gpt3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt3_ick_parent_names),
	.ops		= &gpt3_ick_ops,
};

static struct clk gpt4_fck;

static const struct clk_ops gpt4_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt4_fck_hw = {
	.hw = {
		.clk = &gpt4_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT4_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt4_fck = {
	.name		= "gpt4_fck",
	.hw		= &gpt4_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt4_fck_ops,
};

static struct clk gpt4_ick;

static const char *gpt4_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt4_ick_hw = {
	.hw = {
		.clk = &gpt4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt4_ick = {
	.name		= "gpt4_ick",
	.hw		= &gpt4_ick_hw.hw,
	.parent_names	= gpt4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt4_ick_parent_names),
	.ops		= &gpt4_ick_ops,
};

static struct clk gpt5_fck;

static const struct clk_ops gpt5_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt5_fck_hw = {
	.hw = {
		.clk = &gpt5_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT5_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT5_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt5_fck = {
	.name		= "gpt5_fck",
	.hw		= &gpt5_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt5_fck_ops,
};

static struct clk gpt5_ick;

static const char *gpt5_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt5_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt5_ick_hw = {
	.hw = {
		.clk = &gpt5_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT5_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt5_ick = {
	.name		= "gpt5_ick",
	.hw		= &gpt5_ick_hw.hw,
	.parent_names	= gpt5_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt5_ick_parent_names),
	.ops		= &gpt5_ick_ops,
};

static struct clk gpt6_fck;

static const struct clk_ops gpt6_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt6_fck_hw = {
	.hw = {
		.clk = &gpt6_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT6_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT6_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt6_fck = {
	.name		= "gpt6_fck",
	.hw		= &gpt6_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt6_fck_ops,
};

static struct clk gpt6_ick;

static const char *gpt6_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt6_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt6_ick_hw = {
	.hw = {
		.clk = &gpt6_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT6_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt6_ick = {
	.name		= "gpt6_ick",
	.hw		= &gpt6_ick_hw.hw,
	.parent_names	= gpt6_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt6_ick_parent_names),
	.ops		= &gpt6_ick_ops,
};

static struct clk gpt7_fck;

static const struct clk_ops gpt7_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt7_fck_hw = {
	.hw = {
		.clk = &gpt7_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT7_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT7_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt7_fck = {
	.name		= "gpt7_fck",
	.hw		= &gpt7_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt7_fck_ops,
};

static struct clk gpt7_ick;

static const char *gpt7_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt7_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt7_ick_hw = {
	.hw = {
		.clk = &gpt7_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT7_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt7_ick = {
	.name		= "gpt7_ick",
	.hw		= &gpt7_ick_hw.hw,
	.parent_names	= gpt7_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt7_ick_parent_names),
	.ops		= &gpt7_ick_ops,
};

static struct clk gpt8_fck;

static const struct clk_ops gpt8_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt8_fck_hw = {
	.hw = {
		.clk = &gpt8_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT8_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT8_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt8_fck = {
	.name		= "gpt8_fck",
	.hw		= &gpt8_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt8_fck_ops,
};

static struct clk gpt8_ick;

static const char *gpt8_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt8_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt8_ick_hw = {
	.hw = {
		.clk = &gpt8_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT8_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt8_ick = {
	.name		= "gpt8_ick",
	.hw		= &gpt8_ick_hw.hw,
	.parent_names	= gpt8_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt8_ick_parent_names),
	.ops		= &gpt8_ick_ops,
};

static struct clk gpt9_fck;

static const struct clk_ops gpt9_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt9_fck_hw = {
	.hw = {
		.clk = &gpt9_fck,
	},
	.clksel		= omap24xx_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT9_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT9_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt9_fck = {
	.name		= "gpt9_fck",
	.hw		= &gpt9_fck_hw.hw,
	.parent_names	= gpt10_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt10_fck_parent_names),
	.ops		= &gpt9_fck_ops,
};

static struct clk gpt9_ick;

static const char *gpt9_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops gpt9_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpt9_ick_hw = {
	.hw = {
		.clk = &gpt9_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT9_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk gpt9_ick = {
	.name		= "gpt9_ick",
	.hw		= &gpt9_ick_hw.hw,
	.parent_names	= gpt9_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpt9_ick_parent_names),
	.ops		= &gpt9_ick_ops,
};

static struct clk hdq_fck;

static const char *hdq_fck_parent_names[] = {
	"func_12m_ck",
};

static const struct clk_ops hdq_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap hdq_fck_hw = {
	.hw = {
		.clk = &hdq_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_HDQ_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk hdq_fck = {
	.name		= "hdq_fck",
	.hw		= &hdq_fck_hw.hw,
	.parent_names	= hdq_fck_parent_names,
	.num_parents	= ARRAY_SIZE(hdq_fck_parent_names),
	.ops		= &hdq_fck_ops,
};

static struct clk hdq_ick;

static const char *hdq_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops hdq_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap hdq_ick_hw = {
	.hw = {
		.clk = &hdq_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_HDQ_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk hdq_ick = {
	.name		= "hdq_ick",
	.hw		= &hdq_ick_hw.hw,
	.parent_names	= hdq_ick_parent_names,
	.num_parents	= ARRAY_SIZE(hdq_ick_parent_names),
	.ops		= &hdq_ick_ops,
};

static struct clk i2c1_fck;

static const char *i2c1_fck_parent_names[] = {
	"func_12m_ck",
};

static const struct clk_ops i2c1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c1_fck_hw = {
	.hw = {
		.clk = &i2c1_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_I2C1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c1_fck = {
	.name		= "i2c1_fck",
	.hw		= &i2c1_fck_hw.hw,
	.parent_names	= i2c1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(i2c1_fck_parent_names),
	.ops		= &i2c1_fck_ops,
};

static struct clk i2c1_ick;

static const char *i2c1_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops i2c1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c1_ick_hw = {
	.hw = {
		.clk = &i2c1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_I2C1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c1_ick = {
	.name		= "i2c1_ick",
	.hw		= &i2c1_ick_hw.hw,
	.parent_names	= i2c1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(i2c1_ick_parent_names),
	.ops		= &i2c1_ick_ops,
};

static struct clk i2c2_fck;

static const char *i2c2_fck_parent_names[] = {
	"func_12m_ck",
};

static const struct clk_ops i2c2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c2_fck_hw = {
	.hw = {
		.clk = &i2c2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_I2C2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c2_fck = {
	.name		= "i2c2_fck",
	.hw		= &i2c2_fck_hw.hw,
	.parent_names	= i2c2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(i2c2_fck_parent_names),
	.ops		= &i2c2_fck_ops,
};

static struct clk i2c2_ick;

static const char *i2c2_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops i2c2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c2_ick_hw = {
	.hw = {
		.clk = &i2c2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_I2C2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c2_ick = {
	.name		= "i2c2_ick",
	.hw		= &i2c2_ick_hw.hw,
	.parent_names	= i2c2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(i2c2_ick_parent_names),
	.ops		= &i2c2_ick_ops,
};

static struct clk iva1_ifck;

static const struct clk_ops iva1_ifck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap iva1_ifck_hw = {
	.hw = {
		.clk = &iva1_ifck,
	},
	.clksel		= dsp_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP2420_CLKSEL_IVA_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP2420_EN_IVA_COP_SHIFT,
	.clkdm_name	= "iva1_clkdm",
};

static struct clk iva1_ifck = {
	.name		= "iva1_ifck",
	.hw		= &iva1_ifck_hw.hw,
	.parent_names	= dsp_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dsp_fck_parent_names),
	.ops		= &iva1_ifck_ops,
};

static struct clk iva1_mpu_int_ifck;

static const char *iva1_mpu_int_ifck_parent_names[] = {
	"iva1_ifck",
};

static const struct clk_ops iva1_mpu_int_ifck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap_fixed_divisor_recalc,
};

static struct clk_hw_omap iva1_mpu_int_ifck_hw = {
	.hw = {
		.clk = &iva1_mpu_int_ifck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP2420_EN_IVA_MPU_SHIFT,
	.clkdm_name	= "iva1_clkdm",
	.fixed_div	= 2,
};

static struct clk iva1_mpu_int_ifck = {
	.name		= "iva1_mpu_int_ifck",
	.hw		= &iva1_mpu_int_ifck_hw.hw,
	.parent_names	= iva1_mpu_int_ifck_parent_names,
	.num_parents	= ARRAY_SIZE(iva1_mpu_int_ifck_parent_names),
	.ops		= &iva1_mpu_int_ifck_ops,
};

static struct clk mailboxes_ick;

static const char *mailboxes_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mailboxes_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mailboxes_ick_hw = {
	.hw = {
		.clk = &mailboxes_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MAILBOXES_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mailboxes_ick = {
	.name		= "mailboxes_ick",
	.hw		= &mailboxes_ick_hw.hw,
	.parent_names	= mailboxes_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mailboxes_ick_parent_names),
	.ops		= &mailboxes_ick_ops,
};

static const struct clksel_rate common_mcbsp_96m_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_mcbsp_mcbsp_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel mcbsp_fck_clksel[] = {
	{ .parent = &func_96m_ck, .rates = common_mcbsp_96m_rates },
	{ .parent = &mcbsp_clks, .rates = common_mcbsp_mcbsp_rates },
	{ .parent = NULL },
};

static const char *mcbsp1_fck_parent_names[] = {
	"func_96m_ck",
	"mcbsp_clks",
};

static struct clk mcbsp1_fck;

static const struct clk_ops mcbsp1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap mcbsp1_fck_hw = {
	.hw = {
		.clk = &mcbsp1_fck,
	},
	.clksel		= mcbsp_fck_clksel,
	.clksel_reg	= OMAP242X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP1_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp1_fck = {
	.name		= "mcbsp1_fck",
	.hw		= &mcbsp1_fck_hw.hw,
	.parent_names	= mcbsp1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp1_fck_parent_names),
	.ops		= &mcbsp1_fck_ops,
};

static struct clk mcbsp1_ick;

static const char *mcbsp1_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mcbsp1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcbsp1_ick_hw = {
	.hw = {
		.clk = &mcbsp1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp1_ick = {
	.name		= "mcbsp1_ick",
	.hw		= &mcbsp1_ick_hw.hw,
	.parent_names	= mcbsp1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp1_ick_parent_names),
	.ops		= &mcbsp1_ick_ops,
};

static struct clk mcbsp2_fck;

static const struct clk_ops mcbsp2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap mcbsp2_fck_hw = {
	.hw = {
		.clk = &mcbsp2_fck,
	},
	.clksel		= mcbsp_fck_clksel,
	.clksel_reg	= OMAP242X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP2_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp2_fck = {
	.name		= "mcbsp2_fck",
	.hw		= &mcbsp2_fck_hw.hw,
	.parent_names	= mcbsp1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp1_fck_parent_names),
	.ops		= &mcbsp2_fck_ops,
};

static struct clk mcbsp2_ick;

static const char *mcbsp2_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mcbsp2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcbsp2_ick_hw = {
	.hw = {
		.clk = &mcbsp2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp2_ick = {
	.name		= "mcbsp2_ick",
	.hw		= &mcbsp2_ick_hw.hw,
	.parent_names	= mcbsp2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp2_ick_parent_names),
	.ops		= &mcbsp2_ick_ops,
};

static struct clk mcspi1_fck;

static const char *mcspi1_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops mcspi1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi1_fck_hw = {
	.hw = {
		.clk = &mcspi1_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi1_fck = {
	.name		= "mcspi1_fck",
	.hw		= &mcspi1_fck_hw.hw,
	.parent_names	= mcspi1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi1_fck_parent_names),
	.ops		= &mcspi1_fck_ops,
};

static struct clk mcspi1_ick;

static const char *mcspi1_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mcspi1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi1_ick_hw = {
	.hw = {
		.clk = &mcspi1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi1_ick = {
	.name		= "mcspi1_ick",
	.hw		= &mcspi1_ick_hw.hw,
	.parent_names	= mcspi1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi1_ick_parent_names),
	.ops		= &mcspi1_ick_ops,
};

static struct clk mcspi2_fck;

static const char *mcspi2_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops mcspi2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi2_fck_hw = {
	.hw = {
		.clk = &mcspi2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi2_fck = {
	.name		= "mcspi2_fck",
	.hw		= &mcspi2_fck_hw.hw,
	.parent_names	= mcspi2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi2_fck_parent_names),
	.ops		= &mcspi2_fck_ops,
};

static struct clk mcspi2_ick;

static const char *mcspi2_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mcspi2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi2_ick_hw = {
	.hw = {
		.clk = &mcspi2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi2_ick = {
	.name		= "mcspi2_ick",
	.hw		= &mcspi2_ick_hw.hw,
	.parent_names	= mcspi2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi2_ick_parent_names),
	.ops		= &mcspi2_ick_ops,
};

static struct clk mmc_fck;

static const char *mmc_fck_parent_names[] = {
	"func_96m_ck",
};

static const struct clk_ops mmc_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmc_fck_hw = {
	.hw = {
		.clk = &mmc_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_MMC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmc_fck = {
	.name		= "mmc_fck",
	.hw		= &mmc_fck_hw.hw,
	.parent_names	= mmc_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mmc_fck_parent_names),
	.ops		= &mmc_fck_ops,
};

static struct clk mmc_ick;

static const char *mmc_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mmc_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmc_ick_hw = {
	.hw = {
		.clk = &mmc_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_MMC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmc_ick = {
	.name		= "mmc_ick",
	.hw		= &mmc_ick_hw.hw,
	.parent_names	= mmc_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mmc_ick_parent_names),
	.ops		= &mmc_ick_ops,
};

static const struct clksel_rate mpu_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_242X },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel mpu_clksel[] = {
	{ .parent = &core_ck, .rates = mpu_core_rates },
	{ .parent = NULL },
};

static const char *mpu_ck_parent_names[] = {
	"core_ck",
};

static struct clk mpu_ck;

static const struct clk_ops mpu_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap mpu_ck_hw = {
	.hw = {
		.clk = &mpu_ck,
	},
	.clksel		= mpu_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(MPU_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_MPU_MASK,
	.clkdm_name	= "mpu_clkdm",
};

static struct clk mpu_ck = {
	.name		= "mpu_ck",
	.hw		= &mpu_ck_hw.hw,
	.parent_names	= mpu_ck_parent_names,
	.num_parents	= ARRAY_SIZE(mpu_ck_parent_names),
	.ops		= &mpu_ck_ops,
};

static struct clk mpu_wdt_fck;

static const char *mpu_wdt_fck_parent_names[] = {
	"func_32k_ck",
};

static const struct clk_ops mpu_wdt_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mpu_wdt_fck_hw = {
	.hw = {
		.clk = &mpu_wdt_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_MPU_WDT_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk mpu_wdt_fck = {
	.name		= "mpu_wdt_fck",
	.hw		= &mpu_wdt_fck_hw.hw,
	.parent_names	= mpu_wdt_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mpu_wdt_fck_parent_names),
	.ops		= &mpu_wdt_fck_ops,
};

static struct clk mpu_wdt_ick;

static const char *mpu_wdt_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops mpu_wdt_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mpu_wdt_ick_hw = {
	.hw = {
		.clk = &mpu_wdt_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_MPU_WDT_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk mpu_wdt_ick = {
	.name		= "mpu_wdt_ick",
	.hw		= &mpu_wdt_ick_hw.hw,
	.parent_names	= mpu_wdt_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mpu_wdt_ick_parent_names),
	.ops		= &mpu_wdt_ick_ops,
};

static struct clk mspro_fck;

static const char *mspro_fck_parent_names[] = {
	"func_96m_ck",
};

static const struct clk_ops mspro_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mspro_fck_hw = {
	.hw = {
		.clk = &mspro_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MSPRO_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mspro_fck = {
	.name		= "mspro_fck",
	.hw		= &mspro_fck_hw.hw,
	.parent_names	= mspro_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mspro_fck_parent_names),
	.ops		= &mspro_fck_ops,
};

static struct clk mspro_ick;

static const char *mspro_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops mspro_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mspro_ick_hw = {
	.hw = {
		.clk = &mspro_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MSPRO_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mspro_ick = {
	.name		= "mspro_ick",
	.hw		= &mspro_ick_hw.hw,
	.parent_names	= mspro_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mspro_ick_parent_names),
	.ops		= &mspro_ick_ops,
};

static struct clk omapctrl_ick;

static const char *omapctrl_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops omapctrl_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap omapctrl_ick_hw = {
	.hw = {
		.clk = &omapctrl_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_OMAPCTRL_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk omapctrl_ick = {
	.name		= "omapctrl_ick",
	.hw		= &omapctrl_ick_hw.hw,
	.parent_names	= omapctrl_ick_parent_names,
	.num_parents	= ARRAY_SIZE(omapctrl_ick_parent_names),
	.ops		= &omapctrl_ick_ops,
};

static struct clk pka_ick;

static const char *pka_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops pka_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap pka_ick_hw = {
	.hw = {
		.clk = &pka_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_PKA_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk pka_ick = {
	.name		= "pka_ick",
	.hw		= &pka_ick_hw.hw,
	.parent_names	= pka_ick_parent_names,
	.num_parents	= ARRAY_SIZE(pka_ick_parent_names),
	.ops		= &pka_ick_ops,
};

static struct clk rng_ick;

static const char *rng_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops rng_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap rng_ick_hw = {
	.hw = {
		.clk = &rng_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_RNG_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk rng_ick = {
	.name		= "rng_ick",
	.hw		= &rng_ick_hw.hw,
	.parent_names	= rng_ick_parent_names,
	.num_parents	= ARRAY_SIZE(rng_ick_parent_names),
	.ops		= &rng_ick_ops,
};

static struct clk sdma_fck;

static const char *sdma_fck_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops sdma_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap sdma_fck_hw = {
	.hw = {
		.clk = &sdma_fck,
	},
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk sdma_fck = {
	.name		= "sdma_fck",
	.hw		= &sdma_fck_hw.hw,
	.parent_names	= sdma_fck_parent_names,
	.num_parents	= ARRAY_SIZE(sdma_fck_parent_names),
	.ops		= &sdma_fck_ops,
};

static struct clk sdma_ick;

static const char *sdma_ick_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops sdma_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap sdma_ick_hw = {
	.hw = {
		.clk = &sdma_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_SDMA_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk sdma_ick = {
	.name		= "sdma_ick",
	.hw		= &sdma_ick_hw.hw,
	.parent_names	= sdma_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sdma_ick_parent_names),
	.ops		= &sdma_ick_ops,
};

static struct clk sdrc_ick;

static const char *sdrc_ick_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops sdrc_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap sdrc_ick_hw = {
	.hw = {
		.clk = &sdrc_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_SDRC_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk sdrc_ick = {
	.name		= "sdrc_ick",
	.hw		= &sdrc_ick_hw.hw,
	.parent_names	= sdrc_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sdrc_ick_parent_names),
	.ops		= &sdrc_ick_ops,
};

static struct clk sha_ick;

static const char *sha_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops sha_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sha_ick_hw = {
	.hw = {
		.clk = &sha_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_SHA_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk sha_ick = {
	.name		= "sha_ick",
	.hw		= &sha_ick_hw.hw,
	.parent_names	= sha_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sha_ick_parent_names),
	.ops		= &sha_ick_ops,
};

static struct clk ssi_l4_ick;

static const char *ssi_l4_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops ssi_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap ssi_l4_ick_hw = {
	.hw = {
		.clk = &ssi_l4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_SSI_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_l4_ick = {
	.name		= "ssi_l4_ick",
	.hw		= &ssi_l4_ick_hw.hw,
	.parent_names	= ssi_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_l4_ick_parent_names),
	.ops		= &ssi_l4_ick_ops,
};

static const struct clksel_rate ssi_ssr_sst_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel ssi_ssr_sst_fck_clksel[] = {
	{ .parent = &core_ck, .rates = ssi_ssr_sst_fck_core_rates },
	{ .parent = NULL },
};

static const char *ssi_ssr_sst_fck_parent_names[] = {
	"core_ck",
};

static struct clk ssi_ssr_sst_fck;

static const struct clk_ops ssi_ssr_sst_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap ssi_ssr_sst_fck_hw = {
	.hw = {
		.clk = &ssi_ssr_sst_fck,
	},
	.clksel		= ssi_ssr_sst_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_SSI_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_SSI_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk ssi_ssr_sst_fck = {
	.name		= "ssi_fck",
	.hw		= &ssi_ssr_sst_fck_hw.hw,
	.parent_names	= ssi_ssr_sst_fck_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_ssr_sst_fck_parent_names),
	.ops		= &ssi_ssr_sst_fck_ops,
};

static struct clk sync_32k_ick;

static const char *sync_32k_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops sync_32k_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sync_32k_ick_hw = {
	.hw = {
		.clk = &sync_32k_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_32KSYNC_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sync_32k_ick = {
	.name		= "sync_32k_ick",
	.hw		= &sync_32k_ick_hw.hw,
	.parent_names	= sync_32k_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sync_32k_ick_parent_names),
	.ops		= &sync_32k_ick_ops,
};

static const struct clksel_rate common_clkout_src_core_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_sys_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_96m_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_54m_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel common_clkout_src_clksel[] = {
	{ .parent = &core_ck, .rates = common_clkout_src_core_rates },
	{ .parent = &sys_ck, .rates = common_clkout_src_sys_rates },
	{ .parent = &func_96m_ck, .rates = common_clkout_src_96m_rates },
	{ .parent = &func_54m_ck, .rates = common_clkout_src_54m_rates },
	{ .parent = NULL },
};

static const char *sys_clkout_src_parent_names[] = {
	"core_ck",
	"sys_ck",
	"func_96m_ck",
	"func_54m_ck",
};

static struct clk sys_clkout_src;

static const struct clk_ops sys_clkout_src_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap sys_clkout_src_hw = {
	.hw = {
		.clk = &sys_clkout_src,
	},
	.clksel		= common_clkout_src_clksel,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP24XX_CLKOUT_SOURCE_MASK,
	.enable_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.enable_bit	= OMAP24XX_CLKOUT_EN_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sys_clkout_src = {
	.name		= "sys_clkout_src",
	.hw		= &sys_clkout_src_hw.hw,
	.parent_names	= sys_clkout_src_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout_src_parent_names),
	.ops		= &sys_clkout_src_ops,
};

static const struct clksel_rate common_clkout_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 16, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel sys_clkout_clksel[] = {
	{ .parent = &sys_clkout_src, .rates = common_clkout_rates },
	{ .parent = NULL },
};

static const char *sys_clkout_parent_names[] = {
	"sys_clkout_src",
};

static struct clk sys_clkout;

static const struct clk_ops sys_clkout_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap sys_clkout_hw = {
	.hw = {
		.clk = &sys_clkout,
	},
	.clksel		= sys_clkout_clksel,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP24XX_CLKOUT_DIV_MASK,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sys_clkout = {
	.name		= "sys_clkout",
	.hw		= &sys_clkout_hw.hw,
	.parent_names	= sys_clkout_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout_parent_names),
	.ops		= &sys_clkout_ops,
};

static struct clk sys_clkout2_src;

static const struct clk_ops sys_clkout2_src_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap sys_clkout2_src_hw = {
	.hw = {
		.clk = &sys_clkout2_src,
	},
	.clksel		= common_clkout_src_clksel,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP2420_CLKOUT2_SOURCE_MASK,
	.enable_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.enable_bit	= OMAP2420_CLKOUT2_EN_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sys_clkout2_src = {
	.name		= "sys_clkout2_src",
	.hw		= &sys_clkout2_src_hw.hw,
	.parent_names	= sys_clkout_src_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout_src_parent_names),
	.ops		= &sys_clkout2_src_ops,
};

static const struct clksel sys_clkout2_clksel[] = {
	{ .parent = &sys_clkout2_src, .rates = common_clkout_rates },
	{ .parent = NULL },
};

static const char *sys_clkout2_parent_names[] = {
	"sys_clkout2_src",
};

static struct clk sys_clkout2;

static const struct clk_ops sys_clkout2_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap sys_clkout2_hw = {
	.hw = {
		.clk = &sys_clkout2,
	},
	.clksel		= sys_clkout2_clksel,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP2420_CLKOUT2_DIV_MASK,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sys_clkout2 = {
	.name		= "sys_clkout2",
	.hw		= &sys_clkout2_hw.hw,
	.parent_names	= sys_clkout2_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout2_parent_names),
	.ops		= &sys_clkout2_ops,
};

static struct clk uart1_fck;

static const char *uart1_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops uart1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart1_fck_hw = {
	.hw = {
		.clk = &uart1_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_UART1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart1_fck = {
	.name		= "uart1_fck",
	.hw		= &uart1_fck_hw.hw,
	.parent_names	= uart1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(uart1_fck_parent_names),
	.ops		= &uart1_fck_ops,
};

static struct clk uart1_ick;

static const char *uart1_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops uart1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart1_ick_hw = {
	.hw = {
		.clk = &uart1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_UART1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart1_ick = {
	.name		= "uart1_ick",
	.hw		= &uart1_ick_hw.hw,
	.parent_names	= uart1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(uart1_ick_parent_names),
	.ops		= &uart1_ick_ops,
};

static struct clk uart2_fck;

static const char *uart2_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops uart2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart2_fck_hw = {
	.hw = {
		.clk = &uart2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_UART2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart2_fck = {
	.name		= "uart2_fck",
	.hw		= &uart2_fck_hw.hw,
	.parent_names	= uart2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(uart2_fck_parent_names),
	.ops		= &uart2_fck_ops,
};

static struct clk uart2_ick;

static const char *uart2_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops uart2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart2_ick_hw = {
	.hw = {
		.clk = &uart2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_UART2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart2_ick = {
	.name		= "uart2_ick",
	.hw		= &uart2_ick_hw.hw,
	.parent_names	= uart2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(uart2_ick_parent_names),
	.ops		= &uart2_ick_ops,
};

static struct clk uart3_fck;

static const char *uart3_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops uart3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart3_fck_hw = {
	.hw = {
		.clk = &uart3_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_UART3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart3_fck = {
	.name		= "uart3_fck",
	.hw		= &uart3_fck_hw.hw,
	.parent_names	= uart3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(uart3_fck_parent_names),
	.ops		= &uart3_fck_ops,
};

static struct clk uart3_ick;

static const char *uart3_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops uart3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart3_ick_hw = {
	.hw = {
		.clk = &uart3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_UART3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart3_ick = {
	.name		= "uart3_ick",
	.hw		= &uart3_ick_hw.hw,
	.parent_names	= uart3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(uart3_ick_parent_names),
	.ops		= &uart3_ick_ops,
};

static struct clk usb_fck;

static const char *usb_fck_parent_names[] = {
	"func_48m_ck",
};

static const struct clk_ops usb_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap usb_fck_hw = {
	.hw = {
		.clk = &usb_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_USB_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk usb_fck = {
	.name		= "usb_fck",
	.hw		= &usb_fck_hw.hw,
	.parent_names	= usb_fck_parent_names,
	.num_parents	= ARRAY_SIZE(usb_fck_parent_names),
	.ops		= &usb_fck_ops,
};

static const struct clksel_rate usb_l4_ick_core_l3_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel usb_l4_ick_clksel[] = {
	{ .parent = &core_l3_ck, .rates = usb_l4_ick_core_l3_rates },
	{ .parent = NULL },
};

static const char *usb_l4_ick_parent_names[] = {
	"core_l3_ck",
};

static struct clk usb_l4_ick;

static const struct clk_ops usb_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap usb_l4_ick_hw = {
	.hw = {
		.clk = &usb_l4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.clksel		= usb_l4_ick_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_USB_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_USB_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk usb_l4_ick = {
	.name		= "usb_l4_ick",
	.hw		= &usb_l4_ick_hw.hw,
	.parent_names	= usb_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(usb_l4_ick_parent_names),
	.ops		= &usb_l4_ick_ops,
};

static struct clk virt_prcm_set;

static const char *virt_prcm_set_parent_names[] = {
	"mpu_ck",
};

static const struct clk_ops virt_prcm_set_ops = {
	.recalc_rate	= &omap2_table_mpu_recalc,
	.set_rate	= &omap2_select_table_rate,
	.round_rate	= &omap2_round_to_table_rate,
};

static struct clk_hw_omap virt_prcm_set_hw = {
	.hw = {
		.clk = &virt_prcm_set,
	},
};

static struct clk virt_prcm_set = {
	.name		= "virt_prcm_set",
	.hw		= &virt_prcm_set_hw.hw,
	.parent_names	= virt_prcm_set_parent_names,
	.num_parents	= ARRAY_SIZE(virt_prcm_set_parent_names),
	.ops		= &virt_prcm_set_ops,
};

static const struct clksel_rate vlynq_fck_96m_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel_rate vlynq_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_242X },
	{ .div = 2, .val = 2, .flags = RATE_IN_242X },
	{ .div = 3, .val = 3, .flags = RATE_IN_242X },
	{ .div = 4, .val = 4, .flags = RATE_IN_242X },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 9, .val = 9, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 16, .val = 16, .flags = RATE_IN_242X },
	{ .div = 18, .val = 18, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel vlynq_fck_clksel[] = {
	{ .parent = &func_96m_ck, .rates = vlynq_fck_96m_rates },
	{ .parent = &core_ck, .rates = vlynq_fck_core_rates },
	{ .parent = NULL },
};

static const char *vlynq_fck_parent_names[] = {
	"func_96m_ck",
	"core_ck",
};

static struct clk vlynq_fck;

static const struct clk_ops vlynq_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap vlynq_fck_hw = {
	.hw = {
		.clk = &vlynq_fck,
	},
	.clksel		= vlynq_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP2420_CLKSEL_VLYNQ_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_VLYNQ_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk vlynq_fck = {
	.name		= "vlynq_fck",
	.hw		= &vlynq_fck_hw.hw,
	.parent_names	= vlynq_fck_parent_names,
	.num_parents	= ARRAY_SIZE(vlynq_fck_parent_names),
	.ops		= &vlynq_fck_ops,
};

static struct clk vlynq_ick;

static const char *vlynq_ick_parent_names[] = {
	"core_l3_ck",
};

static const struct clk_ops vlynq_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap vlynq_ick_hw = {
	.hw = {
		.clk = &vlynq_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_VLYNQ_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk vlynq_ick = {
	.name		= "vlynq_ick",
	.hw		= &vlynq_ick_hw.hw,
	.parent_names	= vlynq_ick_parent_names,
	.num_parents	= ARRAY_SIZE(vlynq_ick_parent_names),
	.ops		= &vlynq_ick_ops,
};

static struct clk wdt1_ick;

static const char *wdt1_ick_parent_names[] = {
	"wu_l4_ick",
};

static const struct clk_ops wdt1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt1_ick_hw = {
	.hw = {
		.clk = &wdt1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_WDT1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wdt1_ick = {
	.name		= "wdt1_ick",
	.hw		= &wdt1_ick_hw.hw,
	.parent_names	= wdt1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt1_ick_parent_names),
	.ops		= &wdt1_ick_ops,
};

static struct clk wdt1_osc_ck;

static const char *wdt1_osc_ck_parent_names[] = {
	"osc_ck",
};

static const struct clk_ops wdt1_osc_ck_ops = {
};

static struct clk_hw_omap wdt1_osc_ck_hw = {
	.hw = {
		.clk = &wdt1_osc_ck,
	},
};

static struct clk wdt1_osc_ck = {
	.name		= "ck_wdt1_osc",
	.hw		= &wdt1_osc_ck_hw.hw,
	.parent_names	= wdt1_osc_ck_parent_names,
	.num_parents	= ARRAY_SIZE(wdt1_osc_ck_parent_names),
	.ops		= &wdt1_osc_ck_ops,
};

static struct clk wdt3_fck;

static const char *wdt3_fck_parent_names[] = {
	"func_32k_ck",
};

static const struct clk_ops wdt3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt3_fck_hw = {
	.hw = {
		.clk = &wdt3_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_WDT3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk wdt3_fck = {
	.name		= "wdt3_fck",
	.hw		= &wdt3_fck_hw.hw,
	.parent_names	= wdt3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(wdt3_fck_parent_names),
	.ops		= &wdt3_fck_ops,
};

static struct clk wdt3_ick;

static const char *wdt3_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops wdt3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt3_ick_hw = {
	.hw = {
		.clk = &wdt3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_WDT3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk wdt3_ick = {
	.name		= "wdt3_ick",
	.hw		= &wdt3_ick_hw.hw,
	.parent_names	= wdt3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt3_ick_parent_names),
	.ops		= &wdt3_ick_ops,
};

static struct clk wdt4_fck;

static const char *wdt4_fck_parent_names[] = {
	"func_32k_ck",
};

static const struct clk_ops wdt4_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt4_fck_hw = {
	.hw = {
		.clk = &wdt4_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_WDT4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk wdt4_fck = {
	.name		= "wdt4_fck",
	.hw		= &wdt4_fck_hw.hw,
	.parent_names	= wdt4_fck_parent_names,
	.num_parents	= ARRAY_SIZE(wdt4_fck_parent_names),
	.ops		= &wdt4_fck_ops,
};

static struct clk wdt4_ick;

static const char *wdt4_ick_parent_names[] = {
	"l4_ck",
};

static const struct clk_ops wdt4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt4_ick_hw = {
	.hw = {
		.clk = &wdt4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_WDT4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk wdt4_ick = {
	.name		= "wdt4_ick",
	.hw		= &wdt4_ick_hw.hw,
	.parent_names	= wdt4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt4_ick_parent_names),
	.ops		= &wdt4_ick_ops,
};

/*
 * clkdev integration
 */

static struct omap_clk omap2420_clks[] = {
	/* external root sources */
	CLK(NULL,	"func_32k_ck",	&func_32k_ck,	CK_242X),
	CLK(NULL,	"secure_32k_ck", &secure_32k_ck, CK_242X),
	CLK(NULL,	"osc_ck",	&osc_ck,	CK_242X),
	CLK(NULL,	"sys_ck",	&sys_ck,	CK_242X),
	CLK(NULL,	"alt_ck",	&alt_ck,	CK_242X),
	CLK("omap-mcbsp.1",	"pad_fck",	&mcbsp_clks,	CK_242X),
	CLK("omap-mcbsp.2",	"pad_fck",	&mcbsp_clks,	CK_242X),
	CLK(NULL,	"mcbsp_clks",	&mcbsp_clks,	CK_242X),
	/* internal analog sources */
	CLK(NULL,	"dpll_ck",	&dpll_ck,	CK_242X),
	CLK(NULL,	"apll96_ck",	&apll96_ck,	CK_242X),
	CLK(NULL,	"apll54_ck",	&apll54_ck,	CK_242X),
	/* internal prcm root sources */
	CLK(NULL,	"func_54m_ck",	&func_54m_ck,	CK_242X),
	CLK(NULL,	"core_ck",	&core_ck,	CK_242X),
	CLK("omap-mcbsp.1",	"prcm_fck",	&func_96m_ck,	CK_242X),
	CLK("omap-mcbsp.2",	"prcm_fck",	&func_96m_ck,	CK_242X),
	CLK(NULL,	"func_96m_ck",	&func_96m_ck,	CK_242X),
	CLK(NULL,	"func_48m_ck",	&func_48m_ck,	CK_242X),
	CLK(NULL,	"func_12m_ck",	&func_12m_ck,	CK_242X),
	CLK(NULL,	"ck_wdt1_osc",	&wdt1_osc_ck,	CK_242X),
	CLK(NULL,	"sys_clkout_src", &sys_clkout_src, CK_242X),
	CLK(NULL,	"sys_clkout",	&sys_clkout,	CK_242X),
	CLK(NULL,	"sys_clkout2_src", &sys_clkout2_src, CK_242X),
	CLK(NULL,	"sys_clkout2",	&sys_clkout2,	CK_242X),
	CLK(NULL,	"emul_ck",	&emul_ck,	CK_242X),
	/* mpu domain clocks */
	CLK(NULL,	"mpu_ck",	&mpu_ck,	CK_242X),
	/* dsp domain clocks */
	CLK(NULL,	"dsp_fck",	&dsp_fck,	CK_242X),
	CLK(NULL,	"dsp_ick",	&dsp_ick,	CK_242X),
	CLK(NULL,	"iva1_ifck",	&iva1_ifck,	CK_242X),
	CLK(NULL,	"iva1_mpu_int_ifck", &iva1_mpu_int_ifck, CK_242X),
	/* GFX domain clocks */
	CLK(NULL,	"gfx_3d_fck",	&gfx_3d_fck,	CK_242X),
	CLK(NULL,	"gfx_2d_fck",	&gfx_2d_fck,	CK_242X),
	CLK(NULL,	"gfx_ick",	&gfx_ick,	CK_242X),
	/* DSS domain clocks */
	CLK("omapdss_dss",	"ick",		&dss_ick,	CK_242X),
	CLK(NULL,	"dss1_fck",		&dss1_fck,	CK_242X),
	CLK(NULL,	"dss2_fck",	&dss2_fck,	CK_242X),
	CLK(NULL,	"dss_54m_fck",	&dss_54m_fck,	CK_242X),
	/* L3 domain clocks */
	CLK(NULL,	"core_l3_ck",	&core_l3_ck,	CK_242X),
	CLK(NULL,	"ssi_fck",	&ssi_ssr_sst_fck, CK_242X),
	CLK(NULL,	"usb_l4_ick",	&usb_l4_ick,	CK_242X),
	/* L4 domain clocks */
	CLK(NULL,	"l4_ck",	&l4_ck,		CK_242X),
	CLK(NULL,	"ssi_l4_ick",	&ssi_l4_ick,	CK_242X),
	CLK(NULL,	"wu_l4_ick",	&wu_l4_ick,	CK_242X),
	/* virtual meta-group clock */
	CLK(NULL,	"virt_prcm_set", &virt_prcm_set, CK_242X),
	/* general l4 interface ck, multi-parent functional clk */
	CLK(NULL,	"gpt1_ick",	&gpt1_ick,	CK_242X),
	CLK(NULL,	"gpt1_fck",	&gpt1_fck,	CK_242X),
	CLK(NULL,	"gpt2_ick",	&gpt2_ick,	CK_242X),
	CLK(NULL,	"gpt2_fck",	&gpt2_fck,	CK_242X),
	CLK(NULL,	"gpt3_ick",	&gpt3_ick,	CK_242X),
	CLK(NULL,	"gpt3_fck",	&gpt3_fck,	CK_242X),
	CLK(NULL,	"gpt4_ick",	&gpt4_ick,	CK_242X),
	CLK(NULL,	"gpt4_fck",	&gpt4_fck,	CK_242X),
	CLK(NULL,	"gpt5_ick",	&gpt5_ick,	CK_242X),
	CLK(NULL,	"gpt5_fck",	&gpt5_fck,	CK_242X),
	CLK(NULL,	"gpt6_ick",	&gpt6_ick,	CK_242X),
	CLK(NULL,	"gpt6_fck",	&gpt6_fck,	CK_242X),
	CLK(NULL,	"gpt7_ick",	&gpt7_ick,	CK_242X),
	CLK(NULL,	"gpt7_fck",	&gpt7_fck,	CK_242X),
	CLK(NULL,	"gpt8_ick",	&gpt8_ick,	CK_242X),
	CLK(NULL,	"gpt8_fck",	&gpt8_fck,	CK_242X),
	CLK(NULL,	"gpt9_ick",	&gpt9_ick,	CK_242X),
	CLK(NULL,	"gpt9_fck",	&gpt9_fck,	CK_242X),
	CLK(NULL,	"gpt10_ick",	&gpt10_ick,	CK_242X),
	CLK(NULL,	"gpt10_fck",	&gpt10_fck,	CK_242X),
	CLK(NULL,	"gpt11_ick",	&gpt11_ick,	CK_242X),
	CLK(NULL,	"gpt11_fck",	&gpt11_fck,	CK_242X),
	CLK(NULL,	"gpt12_ick",	&gpt12_ick,	CK_242X),
	CLK(NULL,	"gpt12_fck",	&gpt12_fck,	CK_242X),
	CLK("omap-mcbsp.1", "ick",	&mcbsp1_ick,	CK_242X),
	CLK(NULL,	"mcbsp1_fck",	&mcbsp1_fck,	CK_242X),
	CLK("omap-mcbsp.2", "ick",	&mcbsp2_ick,	CK_242X),
	CLK(NULL,	"mcbsp2_fck",	&mcbsp2_fck,	CK_242X),
	CLK("omap2_mcspi.1", "ick",	&mcspi1_ick,	CK_242X),
	CLK(NULL,	"mcspi1_fck",	&mcspi1_fck,	CK_242X),
	CLK("omap2_mcspi.2", "ick",	&mcspi2_ick,	CK_242X),
	CLK(NULL,	"mcspi2_fck",	&mcspi2_fck,	CK_242X),
	CLK(NULL,	"uart1_ick",	&uart1_ick,	CK_242X),
	CLK(NULL,	"uart1_fck",	&uart1_fck,	CK_242X),
	CLK(NULL,	"uart2_ick",	&uart2_ick,	CK_242X),
	CLK(NULL,	"uart2_fck",	&uart2_fck,	CK_242X),
	CLK(NULL,	"uart3_ick",	&uart3_ick,	CK_242X),
	CLK(NULL,	"uart3_fck",	&uart3_fck,	CK_242X),
	CLK(NULL,	"gpios_ick",	&gpios_ick,	CK_242X),
	CLK(NULL,	"gpios_fck",	&gpios_fck,	CK_242X),
	CLK("omap_wdt",	"ick",		&mpu_wdt_ick,	CK_242X),
	CLK(NULL,	"mpu_wdt_fck",	&mpu_wdt_fck,	CK_242X),
	CLK(NULL,	"sync_32k_ick",	&sync_32k_ick,	CK_242X),
	CLK(NULL,	"wdt1_ick",	&wdt1_ick,	CK_242X),
	CLK(NULL,	"omapctrl_ick",	&omapctrl_ick,	CK_242X),
	CLK("omap24xxcam", "fck",	&cam_fck,	CK_242X),
	CLK("omap24xxcam", "ick",	&cam_ick,	CK_242X),
	CLK(NULL,	"mailboxes_ick", &mailboxes_ick,	CK_242X),
	CLK(NULL,	"wdt4_ick",	&wdt4_ick,	CK_242X),
	CLK(NULL,	"wdt4_fck",	&wdt4_fck,	CK_242X),
	CLK(NULL,	"wdt3_ick",	&wdt3_ick,	CK_242X),
	CLK(NULL,	"wdt3_fck",	&wdt3_fck,	CK_242X),
	CLK(NULL,	"mspro_ick",	&mspro_ick,	CK_242X),
	CLK(NULL,	"mspro_fck",	&mspro_fck,	CK_242X),
	CLK("mmci-omap.0", "ick",	&mmc_ick,	CK_242X),
	CLK("mmci-omap.0", "fck",	&mmc_fck,	CK_242X),
	CLK(NULL,	"fac_ick",	&fac_ick,	CK_242X),
	CLK(NULL,	"fac_fck",	&fac_fck,	CK_242X),
	CLK(NULL,	"eac_ick",	&eac_ick,	CK_242X),
	CLK(NULL,	"eac_fck",	&eac_fck,	CK_242X),
	CLK("omap_hdq.0", "ick",	&hdq_ick,	CK_242X),
	CLK("omap_hdq.0", "fck",	&hdq_fck,	CK_242X),
	CLK("omap_i2c.1", "ick",	&i2c1_ick,	CK_242X),
	CLK(NULL,	"i2c1_fck",	&i2c1_fck,	CK_242X),
	CLK("omap_i2c.2", "ick",	&i2c2_ick,	CK_242X),
	CLK(NULL,	"i2c2_fck",	&i2c2_fck,	CK_242X),
	CLK(NULL,	"gpmc_fck",	&gpmc_fck,	CK_242X),
	CLK(NULL,	"sdma_fck",	&sdma_fck,	CK_242X),
	CLK(NULL,	"sdma_ick",	&sdma_ick,	CK_242X),
	CLK(NULL,	"sdrc_ick",	&sdrc_ick,	CK_242X),
	CLK(NULL,	"vlynq_ick",	&vlynq_ick,	CK_242X),
	CLK(NULL,	"vlynq_fck",	&vlynq_fck,	CK_242X),
	CLK(NULL,	"des_ick",	&des_ick,	CK_242X),
	CLK("omap-sham",	"ick",	&sha_ick,	CK_242X),
	CLK("omap_rng",	"ick",		&rng_ick,	CK_242X),
	CLK("omap-aes",	"ick",	&aes_ick,	CK_242X),
	CLK(NULL,	"pka_ick",	&pka_ick,	CK_242X),
	CLK(NULL,	"usb_fck",	&usb_fck,	CK_242X),
	CLK("musb-hdrc",	"fck",	&osc_ck,	CK_242X),
	CLK("omap_timer.1",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.2",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.3",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.4",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.5",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.6",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.7",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.8",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.9",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.10",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.11",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.12",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.1",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.2",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.3",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.4",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.5",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.6",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.7",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.8",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.9",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.10",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.11",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.12",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.1",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.2",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.3",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.4",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.5",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.6",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.7",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.8",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.9",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.10",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.11",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.12",	"alt_ck",	&alt_ck,	CK_243X),
};

/*
 * init code
 */

int __init omap2420_clk_init(void)
{
	struct omap_clk *c;

	prcm_clksrc_ctrl = OMAP2420_PRCM_CLKSRC_CTRL;
	cm_idlest_pll = OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST);
	cpu_mask = RATE_IN_242X;
	rate_table = omap2420_rate_table;

	for (c = omap2420_clks; c < omap2420_clks + ARRAY_SIZE(omap2420_clks);
	     c++) {
		clkdev_add(&c->lk);
		__clk_init(NULL, c->lk.clk);
	}

	return 0;
}

