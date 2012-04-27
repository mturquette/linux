/*
 * OMAP3 clock data
 *
 * Copyright (C) 2007-2010 Texas Instruments, Inc.
 * Copyright (C) 2007-2011 Nokia Corporation
 *
 * Written by Paul Walmsley
 * With many device clock fixes by Kevin Hilman and Jouni Högander
 * DPLL bypass clock support added by Roman Tereshonkov
 *
 */

/*
 * Virtual clocks are introduced as convenient tools.
 * They are sources for other clocks and not supposed
 * to be requested from drivers directly.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/list.h>
#include <linux/io.h>

#include <plat/hardware.h>
#include <plat/clkdev_omap.h>

#include "iomap.h"
#include "clock.h"
#include "clock3xxx.h"
#include "clock34xx.h"
#include "clock36xx.h"
#include "clock3517.h"
#include "cm2xxx_3xxx.h"
#include "cm-regbits-34xx.h"
#include "prm2xxx_3xxx.h"
#include "prm-regbits-34xx.h"
#include "control.h"

/*
 * clocks
 */

#define OMAP_CM_REGADDR		OMAP34XX_CM_REGADDR

/* Maximum DPLL multiplier, divider values for OMAP3 */
#define OMAP3_MAX_DPLL_MULT		2047
#define OMAP3630_MAX_JTYPE_DPLL_MULT	4095
#define OMAP3_MAX_DPLL_DIV		128

DEFINE_CLK_FIXED_RATE(dummy_apb_pclk, CLK_IS_ROOT, 0x0, 0x0);

DEFINE_CLK_FIXED_RATE(mcbsp_clks, CLK_IS_ROOT, 0x0, 0x0);

DEFINE_CLK_FIXED_RATE(omap_32k_fck, CLK_IS_ROOT, 32768, 0x0);

DEFINE_CLK_FIXED_RATE(pclk_ck, CLK_IS_ROOT, 27000000, 0x0);

DEFINE_CLK_FIXED_RATE(rmii_ck, CLK_IS_ROOT, 50000000, 0x0);

DEFINE_CLK_FIXED_RATE(secure_32k_fck, CLK_IS_ROOT, 32768, 0x0);

DEFINE_CLK_FIXED_RATE(sys_altclk, CLK_IS_ROOT, 0x0, 0x0);

DEFINE_CLK_FIXED_RATE(virt_12m_ck, CLK_IS_ROOT, 12000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_13m_ck, CLK_IS_ROOT, 13000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_16_8m_ck, CLK_IS_ROOT, 16800000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_19_2m_ck, CLK_IS_ROOT, 19200000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_26m_ck, CLK_IS_ROOT, 26000000, 0x0);

DEFINE_CLK_FIXED_RATE(virt_38_4m_ck, CLK_IS_ROOT, 38400000, 0x0);

static const struct clksel_rate osc_sys_12m_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate osc_sys_13m_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate osc_sys_16_8m_rates[] = {
	{ .div = 1, .val = 5, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 0 }
};

static const struct clksel_rate osc_sys_19_2m_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate osc_sys_26m_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate osc_sys_38_4m_rates[] = {
	{ .div = 1, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel osc_sys_clksel[] = {
	{ .parent = &virt_12m_ck, .rates = osc_sys_12m_rates },
	{ .parent = &virt_13m_ck, .rates = osc_sys_13m_rates },
	{ .parent = &virt_16_8m_ck, .rates = osc_sys_16_8m_rates },
	{ .parent = &virt_19_2m_ck, .rates = osc_sys_19_2m_rates },
	{ .parent = &virt_26m_ck, .rates = osc_sys_26m_rates },
	{ .parent = &virt_38_4m_ck, .rates = osc_sys_38_4m_rates },
	{ .parent = NULL },
};

static const char *osc_sys_ck_parent_names[] = {
	"virt_12m_ck",
	"virt_13m_ck",
	"virt_19_2m_ck",
	"virt_26m_ck",
	"virt_38_4m_ck",
	"virt_16_8m_ck",
};

static struct clk osc_sys_ck;

static const struct clk_ops osc_sys_ck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap osc_sys_ck_hw = {
	.hw = {
		.clk = &osc_sys_ck,
	},
	.clksel		= osc_sys_clksel,
	.clksel_reg	= OMAP3430_PRM_CLKSEL,
	.clksel_mask	= OMAP3430_SYS_CLKIN_SEL_MASK,
};

static struct clk osc_sys_ck = {
	.name		= "osc_sys_ck",
	.hw		= &osc_sys_ck_hw.hw,
	.parent_names	= osc_sys_ck_parent_names,
	.num_parents	= ARRAY_SIZE(osc_sys_ck_parent_names),
	.ops		= &osc_sys_ck_ops,
};

static const struct clksel_rate div2_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel sys_clksel[] = {
	{ .parent = &osc_sys_ck, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *sys_ck_parent_names[] = {
	"osc_sys_ck",
};

static struct clk sys_ck;

static const struct clk_ops sys_ck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap sys_ck_hw = {
	.hw = {
		.clk = &sys_ck,
	},
	.clksel		= sys_clksel,
	.clksel_reg	= OMAP3430_PRM_CLKSRC_CTRL,
	.clksel_mask	= OMAP_SYSCLKDIV_MASK,
};

static struct clk sys_ck = {
	.name		= "sys_ck",
	.hw		= &sys_ck_hw.hw,
	.parent_names	= sys_ck_parent_names,
	.num_parents	= ARRAY_SIZE(sys_ck_parent_names),
	.ops		= &sys_ck_ops,
};

static struct dpll_data dpll3_dd = {
	.mult_div1_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.mult_mask	= OMAP3430_CORE_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430_CORE_DPLL_DIV_MASK,
	.clk_bypass	= &sys_ck,
	.clk_ref	= &sys_ck,
	.freqsel_mask	= OMAP3430_CORE_DPLL_FREQSEL_MASK,
	.control_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_mask	= OMAP3430_EN_CORE_DPLL_MASK,
	.auto_recal_bit	= OMAP3430_EN_CORE_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430_CORE_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430_CORE_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_AUTOIDLE),
	.autoidle_mask	= OMAP3430_AUTO_CORE_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST),
	.idlest_mask	= OMAP3430_ST_CORE_CLK_MASK,
	.max_multiplier	= OMAP3_MAX_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
};

static struct clk dpll3_ck;

static const char *dpll3_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll3_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
	.recalc_rate	= &omap3_dpll_recalc,
	.round_rate	= &omap2_dpll_round_rate,
};

static struct clk_hw_omap dpll3_ck_hw = {
	.hw = {
		.clk = &dpll3_ck,
	},
	.dpll_data	= &dpll3_dd,
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_ck = {
	.name		= "dpll3_ck",
	.hw		= &dpll3_ck_hw.hw,
	.parent_names	= dpll3_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_ck_parent_names),
	.ops		= &dpll3_ck_ops,
};

static const struct clksel_rate div31_dpll3_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 5, .val = 5, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 7, .val = 7, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 8, .val = 8, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 9, .val = 9, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 10, .val = 10, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 11, .val = 11, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 12, .val = 12, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 13, .val = 13, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 14, .val = 14, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 15, .val = 15, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 16, .val = 16, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 17, .val = 17, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 18, .val = 18, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 19, .val = 19, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 20, .val = 20, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 21, .val = 21, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 22, .val = 22, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 23, .val = 23, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 24, .val = 24, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 25, .val = 25, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 26, .val = 26, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 27, .val = 27, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 28, .val = 28, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 29, .val = 29, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 30, .val = 30, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 31, .val = 31, .flags = RATE_IN_3430ES2PLUS_36XX },
	{ .div = 0 }
};

static const struct clksel div31_dpll3m2_clksel[] = {
	{ .parent = &dpll3_ck, .rates = div31_dpll3_rates },
	{ .parent = NULL },
};

static const char *dpll3_m2_ck_parent_names[] = {
	"dpll3_ck",
};

static struct clk dpll3_m2_ck;

static const struct clk_ops dpll3_m2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap3_core_dpll_m2_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll3_m2_ck_hw = {
	.hw = {
		.clk = &dpll3_m2_ck,
	},
	.clksel		= div31_dpll3m2_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_CORE_DPLL_CLKOUT_DIV_MASK,
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_m2_ck = {
	.name		= "dpll3_m2_ck",
	.hw		= &dpll3_m2_ck_hw.hw,
	.parent_names	= dpll3_m2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_m2_ck_parent_names),
	.ops		= &dpll3_m2_ck_ops,
};

static struct clk core_ck;

static const char *core_ck_parent_names[] = {
	"dpll3_m2_ck",
};

static const struct clk_ops core_ck_ops = {
};

static struct clk_hw_omap core_ck_hw = {
	.hw = {
		.clk = &core_ck,
	},
};

static struct clk core_ck = {
	.name		= "core_ck",
	.hw		= &core_ck_hw.hw,
	.parent_names	= core_ck_parent_names,
	.num_parents	= ARRAY_SIZE(core_ck_parent_names),
	.ops		= &core_ck_ops,
};

static const struct clksel div2_core_clksel[] = {
	{ .parent = &core_ck, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *l3_ick_parent_names[] = {
	"core_ck",
};

static struct clk l3_ick;

static const struct clk_ops l3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap l3_ick_hw = {
	.hw = {
		.clk = &l3_ick,
	},
	.clksel		= div2_core_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_L3_MASK,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk l3_ick = {
	.name		= "l3_ick",
	.hw		= &l3_ick_hw.hw,
	.parent_names	= l3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(l3_ick_parent_names),
	.ops		= &l3_ick_ops,
};

static const struct clksel div2_l3_clksel[] = {
	{ .parent = &l3_ick, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *l4_ick_parent_names[] = {
	"l3_ick",
};

static struct clk l4_ick;

static const struct clk_ops l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap l4_ick_hw = {
	.hw = {
		.clk = &l4_ick,
	},
	.clksel		= div2_l3_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_L4_MASK,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk l4_ick = {
	.name		= "l4_ick",
	.hw		= &l4_ick_hw.hw,
	.parent_names	= l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(l4_ick_parent_names),
	.ops		= &l4_ick_ops,
};

static struct clk security_l4_ick2;

static const char *security_l4_ick2_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops security_l4_ick2_ops = {
};

static struct clk_hw_omap security_l4_ick2_hw = {
	.hw = {
		.clk = &security_l4_ick2,
	},
};

static struct clk security_l4_ick2 = {
	.name		= "security_l4_ick2",
	.hw		= &security_l4_ick2_hw.hw,
	.parent_names	= security_l4_ick2_parent_names,
	.num_parents	= ARRAY_SIZE(security_l4_ick2_parent_names),
	.ops		= &security_l4_ick2_ops,
};

static struct clk aes1_ick;

static const char *aes1_ick_parent_names[] = {
	"security_l4_ick2",
};

static const struct clk_ops aes1_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap aes1_ick_hw = {
	.hw = {
		.clk = &aes1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP3430_EN_AES1_SHIFT,
};

static struct clk aes1_ick = {
	.name		= "aes1_ick",
	.hw		= &aes1_ick_hw.hw,
	.parent_names	= aes1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(aes1_ick_parent_names),
	.ops		= &aes1_ick_ops,
};

static struct clk core_l4_ick;

static const char *core_l4_ick_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops core_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_l4_ick_hw = {
	.hw = {
		.clk = &core_l4_ick,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk core_l4_ick = {
	.name		= "core_l4_ick",
	.hw		= &core_l4_ick_hw.hw,
	.parent_names	= core_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(core_l4_ick_parent_names),
	.ops		= &core_l4_ick_ops,
};

static struct clk aes2_ick;

static const char *aes2_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops aes2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap aes2_ick_hw = {
	.hw = {
		.clk = &aes2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_AES2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk aes2_ick = {
	.name		= "aes2_ick",
	.hw		= &aes2_ick_hw.hw,
	.parent_names	= aes2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(aes2_ick_parent_names),
	.ops		= &aes2_ick_ops,
};

static struct clk dpll1_fck;

static struct dpll_data dpll1_dd = {
	.mult_div1_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_CLKSEL1_PLL),
	.mult_mask	= OMAP3430_MPU_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430_MPU_DPLL_DIV_MASK,
	.clk_bypass	= &dpll1_fck,
	.clk_ref	= &sys_ck,
	.freqsel_mask	= OMAP3430_MPU_DPLL_FREQSEL_MASK,
	.control_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_CLKEN_PLL),
	.enable_mask	= OMAP3430_EN_MPU_DPLL_MASK,
	.modes		= (1 << DPLL_LOW_POWER_BYPASS) | (1 << DPLL_LOCKED),
	.auto_recal_bit	= OMAP3430_EN_MPU_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430_MPU_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430_MPU_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_AUTOIDLE_PLL),
	.autoidle_mask	= OMAP3430_AUTO_MPU_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_IDLEST_PLL),
	.idlest_mask	= OMAP3430_ST_MPU_CLK_MASK,
	.max_multiplier	= OMAP3_MAX_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
};

static struct clk dpll1_ck;

static const char *dpll1_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll1_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
	.recalc_rate	= &omap3_dpll_recalc,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.round_rate	= &omap2_dpll_round_rate,
};

static struct clk_hw_omap dpll1_ck_hw = {
	.hw = {
		.clk = &dpll1_ck,
	},
	.dpll_data	= &dpll1_dd,
	.clkdm_name	= "dpll1_clkdm",
};

static struct clk dpll1_ck = {
	.name		= "dpll1_ck",
	.hw		= &dpll1_ck_hw.hw,
	.parent_names	= dpll1_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll1_ck_parent_names),
	.ops		= &dpll1_ck_ops,
};

static struct clk dpll1_x2_ck;

static const char *dpll1_x2_ck_parent_names[] = {
	"dpll1_ck",
};

static const struct clk_ops dpll1_x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll1_x2_ck_hw = {
	.hw = {
		.clk = &dpll1_x2_ck,
	},
	.clkdm_name	= "dpll1_clkdm",
};

static struct clk dpll1_x2_ck = {
	.name		= "dpll1_x2_ck",
	.hw		= &dpll1_x2_ck_hw.hw,
	.parent_names	= dpll1_x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll1_x2_ck_parent_names),
	.ops		= &dpll1_x2_ck_ops,
};

static const struct clksel_rate div16_dpll_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 5, .val = 5, .flags = RATE_IN_3XXX },
	{ .div = 6, .val = 6, .flags = RATE_IN_3XXX },
	{ .div = 7, .val = 7, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 8, .flags = RATE_IN_3XXX },
	{ .div = 9, .val = 9, .flags = RATE_IN_3XXX },
	{ .div = 10, .val = 10, .flags = RATE_IN_3XXX },
	{ .div = 11, .val = 11, .flags = RATE_IN_3XXX },
	{ .div = 12, .val = 12, .flags = RATE_IN_3XXX },
	{ .div = 13, .val = 13, .flags = RATE_IN_3XXX },
	{ .div = 14, .val = 14, .flags = RATE_IN_3XXX },
	{ .div = 15, .val = 15, .flags = RATE_IN_3XXX },
	{ .div = 16, .val = 16, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel div16_dpll1_x2m2_clksel[] = {
	{ .parent = &dpll1_x2_ck, .rates = div16_dpll_rates },
	{ .parent = NULL },
};

static const char *dpll1_x2m2_ck_parent_names[] = {
	"dpll1_x2_ck",
};

static struct clk dpll1_x2m2_ck;

static const struct clk_ops dpll1_x2m2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll1_x2m2_ck_hw = {
	.hw = {
		.clk = &dpll1_x2m2_ck,
	},
	.clksel		= div16_dpll1_x2m2_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_CLKSEL2_PLL),
	.clksel_mask	= OMAP3430_MPU_DPLL_CLKOUT_DIV_MASK,
	.clkdm_name	= "dpll1_clkdm",
};

static struct clk dpll1_x2m2_ck = {
	.name		= "dpll1_x2m2_ck",
	.hw		= &dpll1_x2m2_ck_hw.hw,
	.parent_names	= dpll1_x2m2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll1_x2m2_ck_parent_names),
	.ops		= &dpll1_x2m2_ck_ops,
};

static struct clk mpu_ck;

static const char *mpu_ck_parent_names[] = {
	"dpll1_x2m2_ck",
};

static const struct clk_ops mpu_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap mpu_ck_hw = {
	.hw = {
		.clk = &mpu_ck,
	},
	.clkdm_name	= "mpu_clkdm",
};

static struct clk mpu_ck = {
	.name		= "mpu_ck",
	.hw		= &mpu_ck_hw.hw,
	.parent_names	= mpu_ck_parent_names,
	.num_parents	= ARRAY_SIZE(mpu_ck_parent_names),
	.ops		= &mpu_ck_ops,
};

static const struct clksel_rate arm_fck_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel arm_fck_clksel[] = {
	{ .parent = &mpu_ck, .rates = arm_fck_rates },
	{ .parent = NULL },
};

static const char *arm_fck_parent_names[] = {
	"mpu_ck",
};

static struct clk arm_fck;

static const struct clk_ops arm_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap arm_fck_hw = {
	.hw = {
		.clk = &arm_fck,
	},
	.clksel		= arm_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_IDLEST_PLL),
	.clksel_mask	= OMAP3430_ST_MPU_CLK_MASK,
	.clkdm_name	= "mpu_clkdm",
};

static struct clk arm_fck = {
	.name		= "arm_fck",
	.hw		= &arm_fck_hw.hw,
	.parent_names	= arm_fck_parent_names,
	.num_parents	= ARRAY_SIZE(arm_fck_parent_names),
	.ops		= &arm_fck_ops,
};

static const struct clksel_rate emu_src_sys_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate emu_src_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel div16_dpll3_clksel[] = {
	{ .parent = &dpll3_ck, .rates = div16_dpll_rates },
	{ .parent = NULL },
};

static const char *dpll3_m3_ck_parent_names[] = {
	"dpll3_ck",
};

static struct clk dpll3_m3_ck;

static const struct clk_ops dpll3_m3_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll3_m3_ck_hw = {
	.hw = {
		.clk = &dpll3_m3_ck,
	},
	.clksel		= div16_dpll3_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_DIV_DPLL3_MASK,
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_m3_ck = {
	.name		= "dpll3_m3_ck",
	.hw		= &dpll3_m3_ck_hw.hw,
	.parent_names	= dpll3_m3_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_m3_ck_parent_names),
	.ops		= &dpll3_m3_ck_ops,
};

static struct clk dpll3_m3x2_ck;

static const char *dpll3_m3x2_ck_parent_names[] = {
	"dpll3_m3_ck",
};

static const struct clk_ops dpll3_m3x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll3_m3x2_ck_hw = {
	.hw = {
		.clk = &dpll3_m3x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_EMU_CORE_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_m3x2_ck = {
	.name		= "dpll3_m3x2_ck",
	.hw		= &dpll3_m3x2_ck_hw.hw,
	.parent_names	= dpll3_m3x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_m3x2_ck_parent_names),
	.ops		= &dpll3_m3x2_ck_ops,
};

static struct clk emu_core_alwon_ck;

static const char *emu_core_alwon_ck_parent_names[] = {
	"dpll3_m3x2_ck",
};

static const struct clk_ops emu_core_alwon_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap emu_core_alwon_ck_hw = {
	.hw = {
		.clk = &emu_core_alwon_ck,
	},
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk emu_core_alwon_ck = {
	.name		= "emu_core_alwon_ck",
	.hw		= &emu_core_alwon_ck_hw.hw,
	.parent_names	= emu_core_alwon_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emu_core_alwon_ck_parent_names),
	.ops		= &emu_core_alwon_ck_ops,
};

static const struct clksel_rate emu_src_per_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

/* DPLL4 */
/* Supplies 96MHz, 54Mhz TV DAC, DSS fclk, CAM sensor clock, emul trace clk */
/* Type: DPLL */
static struct dpll_data dpll4_dd;

static struct dpll_data dpll4_dd_34xx __initdata = {
	.mult_div1_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL2),
	.mult_mask	= OMAP3430_PERIPH_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430_PERIPH_DPLL_DIV_MASK,
	.clk_bypass	= &sys_ck,
	.clk_ref	= &sys_ck,
	.freqsel_mask	= OMAP3430_PERIPH_DPLL_FREQSEL_MASK,
	.control_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_mask	= OMAP3430_EN_PERIPH_DPLL_MASK,
	.modes		= (1 << DPLL_LOW_POWER_STOP) | (1 << DPLL_LOCKED),
	.auto_recal_bit	= OMAP3430_EN_PERIPH_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430_PERIPH_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430_PERIPH_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_AUTOIDLE),
	.autoidle_mask	= OMAP3430_AUTO_PERIPH_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST),
	.idlest_mask	= OMAP3430_ST_PERIPH_CLK_MASK,
	.max_multiplier = OMAP3_MAX_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
};

static struct dpll_data dpll4_dd_3630 __initdata = {
	.mult_div1_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL2),
	.mult_mask	= OMAP3630_PERIPH_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430_PERIPH_DPLL_DIV_MASK,
	.clk_bypass	= &sys_ck,
	.clk_ref	= &sys_ck,
	.control_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_mask	= OMAP3430_EN_PERIPH_DPLL_MASK,
	.modes		= (1 << DPLL_LOW_POWER_STOP) | (1 << DPLL_LOCKED),
	.auto_recal_bit	= OMAP3430_EN_PERIPH_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430_PERIPH_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430_PERIPH_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_AUTOIDLE),
	.autoidle_mask	= OMAP3430_AUTO_PERIPH_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST),
	.idlest_mask	= OMAP3430_ST_PERIPH_CLK_MASK,
	.dco_mask	= OMAP3630_PERIPH_DPLL_DCO_SEL_MASK,
	.sddiv_mask	= OMAP3630_PERIPH_DPLL_SD_DIV_MASK,
	.max_multiplier = OMAP3630_MAX_JTYPE_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
	.flags		= DPLL_J_TYPE
};

static struct clk dpll4_ck;

static const char *dpll4_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll4_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap3_dpll_recalc,
	.set_rate	= &omap3_dpll4_set_rate,
	.round_rate	= &omap2_dpll_round_rate,
};

static struct clk_hw_omap dpll4_ck_hw = {
	.hw = {
		.clk = &dpll4_ck,
	},
	.dpll_data	= &dpll4_dd,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_ck = {
	.name		= "dpll4_ck",
	.hw		= &dpll4_ck_hw.hw,
	.parent_names	= dpll4_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_ck_parent_names),
	.ops		= &dpll4_ck_ops,
};

static const struct clksel_rate dpll4_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 5, .val = 5, .flags = RATE_IN_3XXX },
	{ .div = 6, .val = 6, .flags = RATE_IN_3XXX },
	{ .div = 7, .val = 7, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 8, .flags = RATE_IN_3XXX },
	{ .div = 9, .val = 9, .flags = RATE_IN_3XXX },
	{ .div = 10, .val = 10, .flags = RATE_IN_3XXX },
	{ .div = 11, .val = 11, .flags = RATE_IN_3XXX },
	{ .div = 12, .val = 12, .flags = RATE_IN_3XXX },
	{ .div = 13, .val = 13, .flags = RATE_IN_3XXX },
	{ .div = 14, .val = 14, .flags = RATE_IN_3XXX },
	{ .div = 15, .val = 15, .flags = RATE_IN_3XXX },
	{ .div = 16, .val = 16, .flags = RATE_IN_3XXX },
	{ .div = 17, .val = 17, .flags = RATE_IN_36XX },
	{ .div = 18, .val = 18, .flags = RATE_IN_36XX },
	{ .div = 19, .val = 19, .flags = RATE_IN_36XX },
	{ .div = 20, .val = 20, .flags = RATE_IN_36XX },
	{ .div = 21, .val = 21, .flags = RATE_IN_36XX },
	{ .div = 22, .val = 22, .flags = RATE_IN_36XX },
	{ .div = 23, .val = 23, .flags = RATE_IN_36XX },
	{ .div = 24, .val = 24, .flags = RATE_IN_36XX },
	{ .div = 25, .val = 25, .flags = RATE_IN_36XX },
	{ .div = 26, .val = 26, .flags = RATE_IN_36XX },
	{ .div = 27, .val = 27, .flags = RATE_IN_36XX },
	{ .div = 28, .val = 28, .flags = RATE_IN_36XX },
	{ .div = 29, .val = 29, .flags = RATE_IN_36XX },
	{ .div = 30, .val = 30, .flags = RATE_IN_36XX },
	{ .div = 31, .val = 31, .flags = RATE_IN_36XX },
	{ .div = 32, .val = 32, .flags = RATE_IN_36XX },
	{ .div = 0 }
};

static const struct clksel dpll4_clksel[] = {
	{ .parent = &dpll4_ck, .rates = dpll4_rates },
	{ .parent = NULL },
};

static const char *dpll4_m6_ck_parent_names[] = {
	"dpll4_ck",
};

static struct clk dpll4_m6_ck;

static const struct clk_ops dpll4_m6_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll4_m6_ck_hw = {
	.hw = {
		.clk = &dpll4_m6_ck,
	},
	.clksel		= dpll4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3630_DIV_DPLL4_MASK,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m6_ck = {
	.name		= "dpll4_m6_ck",
	.hw		= &dpll4_m6_ck_hw.hw,
	.parent_names	= dpll4_m6_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6_ck_parent_names),
	.ops		= &dpll4_m6_ck_ops,
};

static struct clk dpll4_m6x2_ck;

static const char *dpll4_m6x2_ck_parent_names[] = {
	"dpll4_m6_ck",
};

static const struct clk_ops dpll4_m6x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_m6x2_ck_hw = {
	.hw = {
		.clk = &dpll4_m6x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_EMU_PERIPH_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m6x2_ck = {
	.name		= "dpll4_m6x2_ck",
	.hw		= &dpll4_m6x2_ck_hw.hw,
	.parent_names	= dpll4_m6x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6x2_ck_parent_names),
	.ops		= &dpll4_m6x2_ck_ops,
};

static struct clk emu_per_alwon_ck;

static const char *emu_per_alwon_ck_parent_names[] = {
	"dpll4_m6x2_ck",
};

static const struct clk_ops emu_per_alwon_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap emu_per_alwon_ck_hw = {
	.hw = {
		.clk = &emu_per_alwon_ck,
	},
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk emu_per_alwon_ck = {
	.name		= "emu_per_alwon_ck",
	.hw		= &emu_per_alwon_ck_hw.hw,
	.parent_names	= emu_per_alwon_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emu_per_alwon_ck_parent_names),
	.ops		= &emu_per_alwon_ck_ops,
};

static const struct clksel_rate emu_src_mpu_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static struct clk emu_mpu_alwon_ck;

static const char *emu_mpu_alwon_ck_parent_names[] = {
	"mpu_ck",
};

static const struct clk_ops emu_mpu_alwon_ck_ops = {
};

static struct clk_hw_omap emu_mpu_alwon_ck_hw = {
	.hw = {
		.clk = &emu_mpu_alwon_ck,
	},
};

static struct clk emu_mpu_alwon_ck = {
	.name		= "emu_mpu_alwon_ck",
	.hw		= &emu_mpu_alwon_ck_hw.hw,
	.parent_names	= emu_mpu_alwon_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emu_mpu_alwon_ck_parent_names),
	.ops		= &emu_mpu_alwon_ck_ops,
};

static const struct clksel emu_src_clksel[] = {
	{ .parent = &sys_ck, .rates = emu_src_sys_rates },
	{ .parent = &emu_core_alwon_ck, .rates = emu_src_core_rates },
	{ .parent = &emu_per_alwon_ck, .rates = emu_src_per_rates },
	{ .parent = &emu_mpu_alwon_ck, .rates = emu_src_mpu_rates },
	{ .parent = NULL },
};

static const char *emu_src_ck_parent_names[] = {
	"sys_ck",
	"emu_core_alwon_ck",
	"emu_per_alwon_ck",
	"emu_mpu_alwon_ck",
};

static struct clk emu_src_ck;

static const struct clk_ops emu_src_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap emu_src_ck_hw = {
	.hw = {
		.clk = &emu_src_ck,
	},
	.clksel		= emu_src_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_MUX_CTRL_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk emu_src_ck = {
	.name		= "emu_src_ck",
	.hw		= &emu_src_ck_hw.hw,
	.parent_names	= emu_src_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emu_src_ck_parent_names),
	.ops		= &emu_src_ck_ops,
};

static const struct clksel atclk_emu_clksel[] = {
	{ .parent = &emu_src_ck, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *atclk_fck_parent_names[] = {
	"emu_src_ck",
};

static struct clk atclk_fck;

static const struct clk_ops atclk_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap atclk_fck_hw = {
	.hw = {
		.clk = &atclk_fck,
	},
	.clksel		= atclk_emu_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_CLKSEL_ATCLK_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk atclk_fck = {
	.name		= "atclk_fck",
	.hw		= &atclk_fck_hw.hw,
	.parent_names	= atclk_fck_parent_names,
	.num_parents	= ARRAY_SIZE(atclk_fck_parent_names),
	.ops		= &atclk_fck_ops,
};

static struct clk cam_ick;

static const char *cam_ick_parent_names[] = {
	"l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_CAM_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_CAM_SHIFT,
	.clkdm_name	= "cam_clkdm",
};

static struct clk cam_ick = {
	.name		= "cam_ick",
	.hw		= &cam_ick_hw.hw,
	.parent_names	= cam_ick_parent_names,
	.num_parents	= ARRAY_SIZE(cam_ick_parent_names),
	.ops		= &cam_ick_ops,
};

static struct clk dpll4_m5_ck;

static const struct clk_ops dpll4_m5_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll4_m5_ck_hw = {
	.hw = {
		.clk = &dpll4_m5_ck,
	},
	.clksel		= dpll4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_CAM_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3630_CLKSEL_CAM_MASK,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m5_ck = {
	.name		= "dpll4_m5_ck",
	.hw		= &dpll4_m5_ck_hw.hw,
	.parent_names	= dpll4_m6_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6_ck_parent_names),
	.ops		= &dpll4_m5_ck_ops,
};

static struct clk dpll4_m5x2_ck;

static const char *dpll4_m5x2_ck_parent_names[] = {
	"dpll4_m5_ck",
};

static const struct clk_ops dpll4_m5x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_m5x2_ck_hw = {
	.hw = {
		.clk = &dpll4_m5x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_CAM_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m5x2_ck = {
	.name		= "dpll4_m5x2_ck",
	.hw		= &dpll4_m5x2_ck_hw.hw,
	.parent_names	= dpll4_m5x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m5x2_ck_parent_names),
	.ops		= &dpll4_m5x2_ck_ops,
};

static struct clk cam_mclk;

static const char *cam_mclk_parent_names[] = {
	"dpll4_m5x2_ck",
};

static const struct clk_ops cam_mclk_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap cam_mclk_hw = {
	.hw = {
		.clk = &cam_mclk,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_CAM_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_CAM_SHIFT,
	.clkdm_name	= "cam_clkdm",
};

static struct clk cam_mclk = {
	.name		= "cam_mclk",
	.hw		= &cam_mclk_hw.hw,
	.parent_names	= cam_mclk_parent_names,
	.num_parents	= ARRAY_SIZE(cam_mclk_parent_names),
	.ops		= &cam_mclk_ops,
};

static const struct clksel_rate clkout2_src_core_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate clkout2_src_sys_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate clkout2_src_96m_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static struct clk dpll4_m2_ck;

static const struct clk_ops dpll4_m2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll4_m2_ck_hw = {
	.hw = {
		.clk = &dpll4_m2_ck,
	},
	.clksel		= dpll4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, OMAP3430_CM_CLKSEL3),
	.clksel_mask	= OMAP3630_DIV_96M_MASK,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m2_ck = {
	.name		= "dpll4_m2_ck",
	.hw		= &dpll4_m2_ck_hw.hw,
	.parent_names	= dpll4_m6_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6_ck_parent_names),
	.ops		= &dpll4_m2_ck_ops,
};

static struct clk dpll4_m2x2_ck;

static const char *dpll4_m2x2_ck_parent_names[] = {
	"dpll4_m2_ck",
};

static const struct clk_ops dpll4_m2x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_m2x2_ck_hw = {
	.hw = {
		.clk = &dpll4_m2x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_96M_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m2x2_ck = {
	.name		= "dpll4_m2x2_ck",
	.hw		= &dpll4_m2x2_ck_hw.hw,
	.parent_names	= dpll4_m2x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m2x2_ck_parent_names),
	.ops		= &dpll4_m2x2_ck_ops,
};

static struct clk omap_96m_alwon_fck;

static const char *omap_96m_alwon_fck_parent_names[] = {
	"dpll4_m2x2_ck",
};

static const struct clk_ops omap_96m_alwon_fck_ops = {
};

static struct clk_hw_omap omap_96m_alwon_fck_hw = {
	.hw = {
		.clk = &omap_96m_alwon_fck,
	},
};

static struct clk omap_96m_alwon_fck = {
	.name		= "omap_96m_alwon_fck",
	.hw		= &omap_96m_alwon_fck_hw.hw,
	.parent_names	= omap_96m_alwon_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_96m_alwon_fck_parent_names),
	.ops		= &omap_96m_alwon_fck_ops,
};

static struct clk cm_96m_fck;

static const char *cm_96m_fck_parent_names[] = {
	"omap_96m_alwon_fck",
};

static const struct clk_ops cm_96m_fck_ops = {
};

static struct clk_hw_omap cm_96m_fck_hw = {
	.hw = {
		.clk = &cm_96m_fck,
	},
};

static struct clk cm_96m_fck = {
	.name		= "cm_96m_fck",
	.hw		= &cm_96m_fck_hw.hw,
	.parent_names	= cm_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(cm_96m_fck_parent_names),
	.ops		= &cm_96m_fck_ops,
};

static const struct clksel_rate clkout2_src_54m_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate omap_54m_d4m3x2_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static struct clk dpll4_m3_ck;

static const struct clk_ops dpll4_m3_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll4_m3_ck_hw = {
	.hw = {
		.clk = &dpll4_m3_ck,
	},
	.clksel		= dpll4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3630_CLKSEL_TV_MASK,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m3_ck = {
	.name		= "dpll4_m3_ck",
	.hw		= &dpll4_m3_ck_hw.hw,
	.parent_names	= dpll4_m6_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6_ck_parent_names),
	.ops		= &dpll4_m3_ck_ops,
};

static struct clk dpll4_m3x2_ck;

static const char *dpll4_m3x2_ck_parent_names[] = {
	"dpll4_m3_ck",
};

static const struct clk_ops dpll4_m3x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_m3x2_ck_hw = {
	.hw = {
		.clk = &dpll4_m3x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_TV_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m3x2_ck = {
	.name		= "dpll4_m3x2_ck",
	.hw		= &dpll4_m3x2_ck_hw.hw,
	.parent_names	= dpll4_m3x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m3x2_ck_parent_names),
	.ops		= &dpll4_m3x2_ck_ops,
};

static const struct clksel_rate omap_54m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel omap_54m_clksel[] = {
	{ .parent = &dpll4_m3x2_ck, .rates = omap_54m_d4m3x2_rates },
	{ .parent = &sys_altclk, .rates = omap_54m_alt_rates },
	{ .parent = NULL },
};

static const char *omap_54m_fck_parent_names[] = {
	"dpll4_m3x2_ck",
	"sys_altclk",
};

static struct clk omap_54m_fck;

static const struct clk_ops omap_54m_fck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap omap_54m_fck_hw = {
	.hw = {
		.clk = &omap_54m_fck,
	},
	.clksel		= omap_54m_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_SOURCE_54M_MASK,
};

static struct clk omap_54m_fck = {
	.name		= "omap_54m_fck",
	.hw		= &omap_54m_fck_hw.hw,
	.parent_names	= omap_54m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_54m_fck_parent_names),
	.ops		= &omap_54m_fck_ops,
};

static const struct clksel clkout2_src_clksel[] = {
	{ .parent = &core_ck, .rates = clkout2_src_core_rates },
	{ .parent = &sys_ck, .rates = clkout2_src_sys_rates },
	{ .parent = &cm_96m_fck, .rates = clkout2_src_96m_rates },
	{ .parent = &omap_54m_fck, .rates = clkout2_src_54m_rates },
	{ .parent = NULL },
};

static const char *clkout2_src_ck_parent_names[] = {
	"core_ck",
	"sys_ck",
	"cm_96m_fck",
	"omap_54m_fck",
};

static struct clk clkout2_src_ck;

static const struct clk_ops clkout2_src_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap clkout2_src_ck_hw = {
	.hw = {
		.clk = &clkout2_src_ck,
	},
	.clksel		= clkout2_src_clksel,
	.clksel_reg	= OMAP3430_CM_CLKOUT_CTRL,
	.clksel_mask	= OMAP3430_CLKOUT2SOURCE_MASK,
	.enable_reg	= OMAP3430_CM_CLKOUT_CTRL,
	.enable_bit	= OMAP3430_CLKOUT2_EN_SHIFT,
	.clkdm_name	= "core_clkdm",
};

static struct clk clkout2_src_ck = {
	.name		= "clkout2_src_ck",
	.hw		= &clkout2_src_ck_hw.hw,
	.parent_names	= clkout2_src_ck_parent_names,
	.num_parents	= ARRAY_SIZE(clkout2_src_ck_parent_names),
	.ops		= &clkout2_src_ck_ops,
};

static const struct clksel_rate omap_48m_cm96m_rates[] = {
	{ .div = 2, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate omap_48m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel omap_48m_clksel[] = {
	{ .parent = &cm_96m_fck, .rates = omap_48m_cm96m_rates },
	{ .parent = &sys_altclk, .rates = omap_48m_alt_rates },
	{ .parent = NULL },
};

static const char *omap_48m_fck_parent_names[] = {
	"cm_96m_fck",
	"sys_altclk",
};

static struct clk omap_48m_fck;

static const struct clk_ops omap_48m_fck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap omap_48m_fck_hw = {
	.hw = {
		.clk = &omap_48m_fck,
	},
	.clksel		= omap_48m_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_SOURCE_48M_MASK,
};

static struct clk omap_48m_fck = {
	.name		= "omap_48m_fck",
	.hw		= &omap_48m_fck_hw.hw,
	.parent_names	= omap_48m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_48m_fck_parent_names),
	.ops		= &omap_48m_fck_ops,
};

static struct clk omap_12m_fck;

static const char *omap_12m_fck_parent_names[] = {
	"omap_48m_fck",
};

static const struct clk_ops omap_12m_fck_ops = {
	.recalc_rate	= &omap_fixed_divisor_recalc,
};

static struct clk_hw_omap omap_12m_fck_hw = {
	.hw = {
		.clk = &omap_12m_fck,
	},
	.fixed_div	= 4,
};

static struct clk omap_12m_fck = {
	.name		= "omap_12m_fck",
	.hw		= &omap_12m_fck_hw.hw,
	.parent_names	= omap_12m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_12m_fck_parent_names),
	.ops		= &omap_12m_fck_ops,
};

static struct clk core_12m_fck;

static const char *core_12m_fck_parent_names[] = {
	"omap_12m_fck",
};

static const struct clk_ops core_12m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_12m_fck_hw = {
	.hw = {
		.clk = &core_12m_fck,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk core_12m_fck = {
	.name		= "core_12m_fck",
	.hw		= &core_12m_fck_hw.hw,
	.parent_names	= core_12m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(core_12m_fck_parent_names),
	.ops		= &core_12m_fck_ops,
};

static struct clk core_48m_fck;

static const char *core_48m_fck_parent_names[] = {
	"omap_48m_fck",
};

static const struct clk_ops core_48m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_48m_fck_hw = {
	.hw = {
		.clk = &core_48m_fck,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk core_48m_fck = {
	.name		= "core_48m_fck",
	.hw		= &core_48m_fck_hw.hw,
	.parent_names	= core_48m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(core_48m_fck_parent_names),
	.ops		= &core_48m_fck_ops,
};

static const struct clksel_rate omap_96m_dpll_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate omap_96m_sys_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel omap_96m_fck_clksel[] = {
	{ .parent = &cm_96m_fck, .rates = omap_96m_dpll_rates },
	{ .parent = &sys_ck, .rates = omap_96m_sys_rates },
	{ .parent = NULL },
};

static const char *omap_96m_fck_parent_names[] = {
	"cm_96m_fck",
	"sys_ck",
};

static struct clk omap_96m_fck;

static const struct clk_ops omap_96m_fck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap omap_96m_fck_hw = {
	.hw = {
		.clk = &omap_96m_fck,
	},
	.clksel		= omap_96m_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_SOURCE_96M_MASK,
};

static struct clk omap_96m_fck = {
	.name		= "omap_96m_fck",
	.hw		= &omap_96m_fck_hw.hw,
	.parent_names	= omap_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_96m_fck_parent_names),
	.ops		= &omap_96m_fck_ops,
};

static struct clk core_96m_fck;

static const char *core_96m_fck_parent_names[] = {
	"omap_96m_fck",
};

static const struct clk_ops core_96m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_96m_fck_hw = {
	.hw = {
		.clk = &core_96m_fck,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk core_96m_fck = {
	.name		= "core_96m_fck",
	.hw		= &core_96m_fck_hw.hw,
	.parent_names	= core_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(core_96m_fck_parent_names),
	.ops		= &core_96m_fck_ops,
};

static struct clk core_l3_ick;

static const char *core_l3_ick_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops core_l3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap core_l3_ick_hw = {
	.hw = {
		.clk = &core_l3_ick,
	},
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk core_l3_ick = {
	.name		= "core_l3_ick",
	.hw		= &core_l3_ick_hw.hw,
	.parent_names	= core_l3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(core_l3_ick_parent_names),
	.ops		= &core_l3_ick_ops,
};

static struct clk dpll3_m2x2_ck;

static const char *dpll3_m2x2_ck_parent_names[] = {
	"dpll3_m2_ck",
};

static const struct clk_ops dpll3_m2x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll3_m2x2_ck_hw = {
	.hw = {
		.clk = &dpll3_m2x2_ck,
	},
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_m2x2_ck = {
	.name		= "dpll3_m2x2_ck",
	.hw		= &dpll3_m2x2_ck_hw.hw,
	.parent_names	= dpll3_m2x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_m2x2_ck_parent_names),
	.ops		= &dpll3_m2x2_ck_ops,
};

static struct clk corex2_fck;

static const char *corex2_fck_parent_names[] = {
	"dpll3_m2x2_ck",
};

static const struct clk_ops corex2_fck_ops = {
};

static struct clk_hw_omap corex2_fck_hw = {
	.hw = {
		.clk = &corex2_fck,
	},
};

static struct clk corex2_fck = {
	.name		= "corex2_fck",
	.hw		= &corex2_fck_hw.hw,
	.parent_names	= corex2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(corex2_fck_parent_names),
	.ops		= &corex2_fck_ops,
};

static struct clk cpefuse_fck;

static const char *cpefuse_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops cpefuse_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap cpefuse_fck_hw = {
	.hw = {
		.clk = &cpefuse_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP3430ES2_CM_FCLKEN3),
	.enable_bit	= OMAP3430ES2_EN_CPEFUSE_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk cpefuse_fck = {
	.name		= "cpefuse_fck",
	.hw		= &cpefuse_fck_hw.hw,
	.parent_names	= cpefuse_fck_parent_names,
	.num_parents	= ARRAY_SIZE(cpefuse_fck_parent_names),
	.ops		= &cpefuse_fck_ops,
};

static struct clk csi2_96m_fck;

static const char *csi2_96m_fck_parent_names[] = {
	"core_96m_fck",
};

static const struct clk_ops csi2_96m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap csi2_96m_fck_hw = {
	.hw = {
		.clk = &csi2_96m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_CAM_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_CSI2_SHIFT,
	.clkdm_name	= "cam_clkdm",
};

static struct clk csi2_96m_fck = {
	.name		= "csi2_96m_fck",
	.hw		= &csi2_96m_fck_hw.hw,
	.parent_names	= csi2_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(csi2_96m_fck_parent_names),
	.ops		= &csi2_96m_fck_ops,
};

static struct clk d2d_26m_fck;

static const char *d2d_26m_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops d2d_26m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap d2d_26m_fck_hw = {
	.hw = {
		.clk = &d2d_26m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430ES1_EN_D2D_SHIFT,
	.clkdm_name	= "d2d_clkdm",
};

static struct clk d2d_26m_fck = {
	.name		= "d2d_26m_fck",
	.hw		= &d2d_26m_fck_hw.hw,
	.parent_names	= d2d_26m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(d2d_26m_fck_parent_names),
	.ops		= &d2d_26m_fck_ops,
};

static struct clk des1_ick;

static const char *des1_ick_parent_names[] = {
	"security_l4_ick2",
};

static const struct clk_ops des1_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap des1_ick_hw = {
	.hw = {
		.clk = &des1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP3430_EN_DES1_SHIFT,
};

static struct clk des1_ick = {
	.name		= "des1_ick",
	.hw		= &des1_ick_hw.hw,
	.parent_names	= des1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(des1_ick_parent_names),
	.ops		= &des1_ick_ops,
};

static struct clk des2_ick;

static const char *des2_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops des2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap des2_ick_hw = {
	.hw = {
		.clk = &des2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_DES2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk des2_ick = {
	.name		= "des2_ick",
	.hw		= &des2_ick_hw.hw,
	.parent_names	= des2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(des2_ick_parent_names),
	.ops		= &des2_ick_ops,
};

static const struct clksel_rate div4_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel div4_core_clksel[] = {
	{ .parent = &core_ck, .rates = div4_rates },
	{ .parent = NULL },
};

static const char *dpll1_fck_parent_names[] = {
	"core_ck",
};

static struct clk dpll1_fck;

static const struct clk_ops dpll1_fck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll1_fck_hw = {
	.hw = {
		.clk = &dpll1_fck,
	},
	.clksel		= div4_core_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(MPU_MOD, OMAP3430_CM_CLKSEL1_PLL),
	.clksel_mask	= OMAP3430_MPU_CLK_SRC_MASK,
};

static struct clk dpll1_fck = {
	.name		= "dpll1_fck",
	.hw		= &dpll1_fck_hw.hw,
	.parent_names	= dpll1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll1_fck_parent_names),
	.ops		= &dpll1_fck_ops,
};

static struct clk dpll2_fck;

static struct dpll_data dpll2_dd = {
	.mult_div1_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_CLKSEL1_PLL),
	.mult_mask	= OMAP3430_IVA2_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430_IVA2_DPLL_DIV_MASK,
	.clk_bypass	= &dpll2_fck,
	.clk_ref	= &sys_ck,
	.freqsel_mask	= OMAP3430_IVA2_DPLL_FREQSEL_MASK,
	.control_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_CLKEN_PLL),
	.enable_mask	= OMAP3430_EN_IVA2_DPLL_MASK,
	.modes		= (1 << DPLL_LOW_POWER_STOP) | (1 << DPLL_LOCKED) | (1 << DPLL_LOW_POWER_BYPASS),
	.auto_recal_bit	= OMAP3430_EN_IVA2_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430_PRM_IRQENABLE_MPU_IVA2_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430_PRM_IRQSTATUS_MPU_IVA2_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_AUTOIDLE_PLL),
	.autoidle_mask	= OMAP3430_AUTO_IVA2_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_IDLEST_PLL),
	.idlest_mask	= OMAP3430_ST_IVA2_CLK_MASK,
	.max_multiplier	= OMAP3_MAX_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
};

static struct clk dpll2_ck;

static const char *dpll2_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
	.recalc_rate	= &omap3_dpll_recalc,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.round_rate	= &omap2_dpll_round_rate,
};

static struct clk_hw_omap dpll2_ck_hw = {
	.hw = {
		.clk = &dpll2_ck,
	},
	.dpll_data	= &dpll2_dd,
	.clkdm_name	= "dpll2_clkdm",
};

static struct clk dpll2_ck = {
	.name		= "dpll2_ck",
	.hw		= &dpll2_ck_hw.hw,
	.parent_names	= dpll2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll2_ck_parent_names),
	.ops		= &dpll2_ck_ops,
};

static struct clk dpll2_fck;

static const struct clk_ops dpll2_fck_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll2_fck_hw = {
	.hw = {
		.clk = &dpll2_fck,
	},
	.clksel		= div4_core_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_CLKSEL1_PLL),
	.clksel_mask	= OMAP3430_IVA2_CLK_SRC_MASK,
};

static struct clk dpll2_fck = {
	.name		= "dpll2_fck",
	.hw		= &dpll2_fck_hw.hw,
	.parent_names	= dpll1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll1_fck_parent_names),
	.ops		= &dpll2_fck_ops,
};

static const struct clksel div16_dpll2_m2x2_clksel[] = {
	{ .parent = &dpll2_ck, .rates = div16_dpll_rates },
	{ .parent = NULL },
};

static const char *dpll2_m2_ck_parent_names[] = {
	"dpll2_ck",
};

static struct clk dpll2_m2_ck;

static const struct clk_ops dpll2_m2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll2_m2_ck_hw = {
	.hw = {
		.clk = &dpll2_m2_ck,
	},
	.clksel		= div16_dpll2_m2x2_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, OMAP3430_CM_CLKSEL2_PLL),
	.clksel_mask	= OMAP3430_IVA2_DPLL_CLKOUT_DIV_MASK,
	.clkdm_name	= "dpll2_clkdm",
};

static struct clk dpll2_m2_ck = {
	.name		= "dpll2_m2_ck",
	.hw		= &dpll2_m2_ck_hw.hw,
	.parent_names	= dpll2_m2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll2_m2_ck_parent_names),
	.ops		= &dpll2_m2_ck_ops,
};

static struct clk dpll3_x2_ck;

static const char *dpll3_x2_ck_parent_names[] = {
	"dpll3_ck",
};

static const struct clk_ops dpll3_x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll3_x2_ck_hw = {
	.hw = {
		.clk = &dpll3_x2_ck,
	},
	.clkdm_name	= "dpll3_clkdm",
};

static struct clk dpll3_x2_ck = {
	.name		= "dpll3_x2_ck",
	.hw		= &dpll3_x2_ck_hw.hw,
	.parent_names	= dpll3_x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll3_x2_ck_parent_names),
	.ops		= &dpll3_x2_ck_ops,
};

static struct clk dpll4_m4_ck;

static const struct clk_ops dpll4_m4_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll4_m4_ck_hw = {
	.hw = {
		.clk = &dpll4_m4_ck,
	},
	.clksel		= dpll4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3630_CLKSEL_DSS1_MASK,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m4_ck = {
	.name		= "dpll4_m4_ck",
	.hw		= &dpll4_m4_ck_hw.hw,
	.parent_names	= dpll4_m6_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m6_ck_parent_names),
	.ops		= &dpll4_m4_ck_ops,
};

static struct clk dpll4_m4x2_ck;

static const char *dpll4_m4x2_ck_parent_names[] = {
	"dpll4_m4_ck",
};

static const struct clk_ops dpll4_m4x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_m4x2_ck_hw = {
	.hw = {
		.clk = &dpll4_m4x2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP3430_PWRDN_DSS1_SHIFT,
	.flags		= INVERT_ENABLE,
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_m4x2_ck = {
	.name		= "dpll4_m4x2_ck",
	.hw		= &dpll4_m4x2_ck_hw.hw,
	.parent_names	= dpll4_m4x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_m4x2_ck_parent_names),
	.ops		= &dpll4_m4x2_ck_ops,
};

static struct clk dpll4_x2_ck;

static const char *dpll4_x2_ck_parent_names[] = {
	"dpll4_ck",
};

static const struct clk_ops dpll4_x2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap3_clkoutx2_recalc,
};

static struct clk_hw_omap dpll4_x2_ck_hw = {
	.hw = {
		.clk = &dpll4_x2_ck,
	},
	.clkdm_name	= "dpll4_clkdm",
};

static struct clk dpll4_x2_ck = {
	.name		= "dpll4_x2_ck",
	.hw		= &dpll4_x2_ck_hw.hw,
	.parent_names	= dpll4_x2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll4_x2_ck_parent_names),
	.ops		= &dpll4_x2_ck_ops,
};

static struct dpll_data dpll5_dd = {
	.mult_div1_reg	= OMAP_CM_REGADDR(PLL_MOD, OMAP3430ES2_CM_CLKSEL4),
	.mult_mask	= OMAP3430ES2_PERIPH2_DPLL_MULT_MASK,
	.div1_mask	= OMAP3430ES2_PERIPH2_DPLL_DIV_MASK,
	.clk_bypass	= &sys_ck,
	.clk_ref	= &sys_ck,
	.freqsel_mask	= OMAP3430ES2_PERIPH2_DPLL_FREQSEL_MASK,
	.control_reg	= OMAP_CM_REGADDR(PLL_MOD, OMAP3430ES2_CM_CLKEN2),
	.enable_mask	= OMAP3430ES2_EN_PERIPH2_DPLL_MASK,
	.modes		= (1 << DPLL_LOW_POWER_STOP) | (1 << DPLL_LOCKED),
	.auto_recal_bit	= OMAP3430ES2_EN_PERIPH2_DPLL_DRIFTGUARD_SHIFT,
	.recal_en_bit	= OMAP3430ES2_SND_PERIPH_DPLL_RECAL_EN_SHIFT,
	.recal_st_bit	= OMAP3430ES2_SND_PERIPH_DPLL_ST_SHIFT,
	.autoidle_reg	= OMAP_CM_REGADDR(PLL_MOD, OMAP3430ES2_CM_AUTOIDLE2_PLL),
	.autoidle_mask	= OMAP3430ES2_AUTO_PERIPH2_DPLL_MASK,
	.idlest_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST2),
	.idlest_mask	= OMAP3430ES2_ST_PERIPH2_CLK_MASK,
	.max_multiplier	= OMAP3_MAX_DPLL_MULT,
	.min_divider	= 1,
	.max_divider	= OMAP3_MAX_DPLL_DIV,
};

static struct clk dpll5_ck;

static const char *dpll5_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dpll5_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.get_parent	= &omap2_init_dpll_parent,
	.recalc_rate	= &omap3_dpll_recalc,
	.set_rate	= &omap3_noncore_dpll_set_rate,
	.round_rate	= &omap2_dpll_round_rate,
};

static struct clk_hw_omap dpll5_ck_hw = {
	.hw = {
		.clk = &dpll5_ck,
	},
	.dpll_data	= &dpll5_dd,
	.clkdm_name	= "dpll5_clkdm",
};

static struct clk dpll5_ck = {
	.name		= "dpll5_ck",
	.hw		= &dpll5_ck_hw.hw,
	.parent_names	= dpll5_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll5_ck_parent_names),
	.ops		= &dpll5_ck_ops,
};

static const struct clksel div16_dpll5_clksel[] = {
	{ .parent = &dpll5_ck, .rates = div16_dpll_rates },
	{ .parent = NULL },
};

static const char *dpll5_m2_ck_parent_names[] = {
	"dpll5_ck",
};

static struct clk dpll5_m2_ck;

static const struct clk_ops dpll5_m2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap dpll5_m2_ck_hw = {
	.hw = {
		.clk = &dpll5_m2_ck,
	},
	.clksel		= div16_dpll5_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, OMAP3430ES2_CM_CLKSEL5),
	.clksel_mask	= OMAP3430ES2_DIV_120M_MASK,
	.clkdm_name	= "dpll5_clkdm",
};

static struct clk dpll5_m2_ck = {
	.name		= "dpll5_m2_ck",
	.hw		= &dpll5_m2_ck_hw.hw,
	.parent_names	= dpll5_m2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(dpll5_m2_ck_parent_names),
	.ops		= &dpll5_m2_ck_ops,
};

static struct clk dss1_alwon_fck_3430es1;

static const char *dss1_alwon_fck_3430es1_parent_names[] = {
	"dpll4_m4x2_ck",
};

static const struct clk_ops dss1_alwon_fck_3430es1_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss1_alwon_fck_3430es1_hw = {
	.hw = {
		.clk = &dss1_alwon_fck_3430es1,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_DSS1_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss1_alwon_fck_3430es1 = {
	.name		= "dss1_alwon_fck",
	.hw		= &dss1_alwon_fck_3430es1_hw.hw,
	.parent_names	= dss1_alwon_fck_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(dss1_alwon_fck_3430es1_parent_names),
	.ops		= &dss1_alwon_fck_3430es1_ops,
};

static struct clk dss1_alwon_fck_3430es2;

static const char *dss1_alwon_fck_3430es2_parent_names[] = {
	"dpll4_m4x2_ck",
};

static const struct clk_ops dss1_alwon_fck_3430es2_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap dss1_alwon_fck_3430es2_hw = {
	.hw = {
		.clk = &dss1_alwon_fck_3430es2,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_DSS1_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss1_alwon_fck_3430es2 = {
	.name		= "dss1_alwon_fck",
	.hw		= &dss1_alwon_fck_3430es2_hw.hw,
	.parent_names	= dss1_alwon_fck_3430es2_parent_names,
	.num_parents	= ARRAY_SIZE(dss1_alwon_fck_3430es2_parent_names),
	.ops		= &dss1_alwon_fck_3430es2_ops,
};

static struct clk dss2_alwon_fck;

static const char *dss2_alwon_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops dss2_alwon_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss2_alwon_fck_hw = {
	.hw = {
		.clk = &dss2_alwon_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_DSS2_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss2_alwon_fck = {
	.name		= "dss2_alwon_fck",
	.hw		= &dss2_alwon_fck_hw.hw,
	.parent_names	= dss2_alwon_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss2_alwon_fck_parent_names),
	.ops		= &dss2_alwon_fck_ops,
};

static struct clk dss_96m_fck;

static const char *dss_96m_fck_parent_names[] = {
	"omap_96m_fck",
};

static const struct clk_ops dss_96m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss_96m_fck_hw = {
	.hw = {
		.clk = &dss_96m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_TV_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_96m_fck = {
	.name		= "dss_96m_fck",
	.hw		= &dss_96m_fck_hw.hw,
	.parent_names	= dss_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss_96m_fck_parent_names),
	.ops		= &dss_96m_fck_ops,
};

static struct clk dss_ick_3430es1;

static const char *dss_ick_3430es1_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops dss_ick_3430es1_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss_ick_3430es1_hw = {
	.hw = {
		.clk = &dss_ick_3430es1,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_CM_ICLKEN_DSS_EN_DSS_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_ick_3430es1 = {
	.name		= "dss_ick",
	.hw		= &dss_ick_3430es1_hw.hw,
	.parent_names	= dss_ick_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(dss_ick_3430es1_parent_names),
	.ops		= &dss_ick_3430es1_ops,
};

static struct clk dss_ick_3430es2;

static const char *dss_ick_3430es2_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops dss_ick_3430es2_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap dss_ick_3430es2_hw = {
	.hw = {
		.clk = &dss_ick_3430es2,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_CM_ICLKEN_DSS_EN_DSS_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_ick_3430es2 = {
	.name		= "dss_ick",
	.hw		= &dss_ick_3430es2_hw.hw,
	.parent_names	= dss_ick_3430es2_parent_names,
	.num_parents	= ARRAY_SIZE(dss_ick_3430es2_parent_names),
	.ops		= &dss_ick_3430es2_ops,
};

static struct clk dss_tv_fck;

static const char *dss_tv_fck_parent_names[] = {
	"omap_54m_fck",
};

static const struct clk_ops dss_tv_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap dss_tv_fck_hw = {
	.hw = {
		.clk = &dss_tv_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_DSS_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_TV_SHIFT,
	.clkdm_name	= "dss_clkdm",
};

static struct clk dss_tv_fck = {
	.name		= "dss_tv_fck",
	.hw		= &dss_tv_fck_hw.hw,
	.parent_names	= dss_tv_fck_parent_names,
	.num_parents	= ARRAY_SIZE(dss_tv_fck_parent_names),
	.ops		= &dss_tv_fck_ops,
};

static struct clk emac_fck;

static const char *emac_fck_parent_names[] = {
	"rmii_ck",
};

static const struct clk_ops emac_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap emac_fck_hw = {
	.hw = {
		.clk = &emac_fck,
	},
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_CPGMAC_FCLK_SHIFT,
};

static struct clk emac_fck = {
	.name		= "emac_fck",
	.hw		= &emac_fck_hw.hw,
	.parent_names	= emac_fck_parent_names,
	.num_parents	= ARRAY_SIZE(emac_fck_parent_names),
	.ops		= &emac_fck_ops,
};

static struct clk ipss_ick;

static const char *ipss_ick_parent_names[] = {
	"core_l3_ick",
};

static const struct clk_ops ipss_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap ipss_ick_hw = {
	.hw = {
		.clk = &ipss_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= AM35XX_EN_IPSS_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk ipss_ick = {
	.name		= "ipss_ick",
	.hw		= &ipss_ick_hw.hw,
	.parent_names	= ipss_ick_parent_names,
	.num_parents	= ARRAY_SIZE(ipss_ick_parent_names),
	.ops		= &ipss_ick_ops,
};

static struct clk emac_ick;

static const char *emac_ick_parent_names[] = {
	"ipss_ick",
};

static const struct clk_ops emac_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap emac_ick_hw = {
	.hw = {
		.clk = &emac_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_CPGMAC_VBUSP_CLK_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk emac_ick = {
	.name		= "emac_ick",
	.hw		= &emac_ick_hw.hw,
	.parent_names	= emac_ick_parent_names,
	.num_parents	= ARRAY_SIZE(emac_ick_parent_names),
	.ops		= &emac_ick_ops,
};

static struct clk fac_ick;

static const char *fac_ick_parent_names[] = {
	"core_l4_ick",
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
	.enable_bit	= OMAP3430ES1_EN_FAC_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk fac_ick = {
	.name		= "fac_ick",
	.hw		= &fac_ick_hw.hw,
	.parent_names	= fac_ick_parent_names,
	.num_parents	= ARRAY_SIZE(fac_ick_parent_names),
	.ops		= &fac_ick_ops,
};

static struct clk fshostusb_fck;

static const char *fshostusb_fck_parent_names[] = {
	"core_48m_fck",
};

static const struct clk_ops fshostusb_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap fshostusb_fck_hw = {
	.hw = {
		.clk = &fshostusb_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430ES1_EN_FSHOSTUSB_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk fshostusb_fck = {
	.name		= "fshostusb_fck",
	.hw		= &fshostusb_fck_hw.hw,
	.parent_names	= fshostusb_fck_parent_names,
	.num_parents	= ARRAY_SIZE(fshostusb_fck_parent_names),
	.ops		= &fshostusb_fck_ops,
};

static struct clk gfx_l3_ck;

static const char *gfx_l3_ck_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops gfx_l3_ck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gfx_l3_ck_hw = {
	.hw = {
		.clk = &gfx_l3_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_ICLKEN),
	.enable_bit	= OMAP_EN_GFX_SHIFT,
};

static struct clk gfx_l3_ck = {
	.name		= "gfx_l3_ck",
	.hw		= &gfx_l3_ck_hw.hw,
	.parent_names	= gfx_l3_ck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_l3_ck_parent_names),
	.ops		= &gfx_l3_ck_ops,
};

static const struct clksel gfx_l3_clksel[] = {
	{ .parent = &l3_ick, .rates = gfx_l3_rates },
	{ .parent = NULL },
};

static const char *gfx_l3_fck_parent_names[] = {
	"l3_ick",
};

static struct clk gfx_l3_fck;

static const struct clk_ops gfx_l3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap gfx_l3_fck_hw = {
	.hw = {
		.clk = &gfx_l3_fck,
	},
	.clksel		= gfx_l3_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP_CLKSEL_GFX_MASK,
	.clkdm_name	= "gfx_3430es1_clkdm",
};

static struct clk gfx_l3_fck = {
	.name		= "gfx_l3_fck",
	.hw		= &gfx_l3_fck_hw.hw,
	.parent_names	= gfx_l3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_l3_fck_parent_names),
	.ops		= &gfx_l3_fck_ops,
};

static struct clk gfx_cg1_ck;

static const char *gfx_cg1_ck_parent_names[] = {
	"gfx_l3_fck",
};

static const struct clk_ops gfx_cg1_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gfx_cg1_ck_hw = {
	.hw = {
		.clk = &gfx_cg1_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES1_EN_2D_SHIFT,
	.clkdm_name	= "gfx_3430es1_clkdm",
};

static struct clk gfx_cg1_ck = {
	.name		= "gfx_cg1_ck",
	.hw		= &gfx_cg1_ck_hw.hw,
	.parent_names	= gfx_cg1_ck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_cg1_ck_parent_names),
	.ops		= &gfx_cg1_ck_ops,
};

static struct clk gfx_cg2_ck;

static const char *gfx_cg2_ck_parent_names[] = {
	"gfx_l3_fck",
};

static const struct clk_ops gfx_cg2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gfx_cg2_ck_hw = {
	.hw = {
		.clk = &gfx_cg2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES1_EN_3D_SHIFT,
	.clkdm_name	= "gfx_3430es1_clkdm",
};

static struct clk gfx_cg2_ck = {
	.name		= "gfx_cg2_ck",
	.hw		= &gfx_cg2_ck_hw.hw,
	.parent_names	= gfx_cg2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_cg2_ck_parent_names),
	.ops		= &gfx_cg2_ck_ops,
};

static struct clk gfx_l3_ick;

static const char *gfx_l3_ick_parent_names[] = {
	"gfx_l3_ck",
};

static const struct clk_ops gfx_l3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap gfx_l3_ick_hw = {
	.hw = {
		.clk = &gfx_l3_ick,
	},
	.clkdm_name	= "gfx_3430es1_clkdm",
};

static struct clk gfx_l3_ick = {
	.name		= "gfx_l3_ick",
	.hw		= &gfx_l3_ick_hw.hw,
	.parent_names	= gfx_l3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gfx_l3_ick_parent_names),
	.ops		= &gfx_l3_ick_ops,
};

static struct clk wkup_32k_fck;

static const char *wkup_32k_fck_parent_names[] = {
	"omap_32k_fck",
};

static const struct clk_ops wkup_32k_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap wkup_32k_fck_hw = {
	.hw = {
		.clk = &wkup_32k_fck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wkup_32k_fck = {
	.name		= "wkup_32k_fck",
	.hw		= &wkup_32k_fck_hw.hw,
	.parent_names	= wkup_32k_fck_parent_names,
	.num_parents	= ARRAY_SIZE(wkup_32k_fck_parent_names),
	.ops		= &wkup_32k_fck_ops,
};

static struct clk gpio1_dbck;

static const char *gpio1_dbck_parent_names[] = {
	"wkup_32k_fck",
};

static const struct clk_ops gpio1_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio1_dbck_hw = {
	.hw = {
		.clk = &gpio1_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpio1_dbck = {
	.name		= "gpio1_dbck",
	.hw		= &gpio1_dbck_hw.hw,
	.parent_names	= gpio1_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio1_dbck_parent_names),
	.ops		= &gpio1_dbck_ops,
};

static struct clk wkup_l4_ick;

static const char *wkup_l4_ick_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops wkup_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap wkup_l4_ick_hw = {
	.hw = {
		.clk = &wkup_l4_ick,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wkup_l4_ick = {
	.name		= "wkup_l4_ick",
	.hw		= &wkup_l4_ick_hw.hw,
	.parent_names	= wkup_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wkup_l4_ick_parent_names),
	.ops		= &wkup_l4_ick_ops,
};

static struct clk gpio1_ick;

static const char *gpio1_ick_parent_names[] = {
	"wkup_l4_ick",
};

static const struct clk_ops gpio1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio1_ick_hw = {
	.hw = {
		.clk = &gpio1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpio1_ick = {
	.name		= "gpio1_ick",
	.hw		= &gpio1_ick_hw.hw,
	.parent_names	= gpio1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio1_ick_parent_names),
	.ops		= &gpio1_ick_ops,
};

static struct clk per_32k_alwon_fck;

static const char *per_32k_alwon_fck_parent_names[] = {
	"omap_32k_fck",
};

static const struct clk_ops per_32k_alwon_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap per_32k_alwon_fck_hw = {
	.hw = {
		.clk = &per_32k_alwon_fck,
	},
	.clkdm_name	= "per_clkdm",
};

static struct clk per_32k_alwon_fck = {
	.name		= "per_32k_alwon_fck",
	.hw		= &per_32k_alwon_fck_hw.hw,
	.parent_names	= per_32k_alwon_fck_parent_names,
	.num_parents	= ARRAY_SIZE(per_32k_alwon_fck_parent_names),
	.ops		= &per_32k_alwon_fck_ops,
};

static struct clk gpio2_dbck;

static const char *gpio2_dbck_parent_names[] = {
	"per_32k_alwon_fck",
};

static const struct clk_ops gpio2_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio2_dbck_hw = {
	.hw = {
		.clk = &gpio2_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO2_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio2_dbck = {
	.name		= "gpio2_dbck",
	.hw		= &gpio2_dbck_hw.hw,
	.parent_names	= gpio2_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio2_dbck_parent_names),
	.ops		= &gpio2_dbck_ops,
};

static struct clk per_l4_ick;

static const char *per_l4_ick_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops per_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap per_l4_ick_hw = {
	.hw = {
		.clk = &per_l4_ick,
	},
	.clkdm_name	= "per_clkdm",
};

static struct clk per_l4_ick = {
	.name		= "per_l4_ick",
	.hw		= &per_l4_ick_hw.hw,
	.parent_names	= per_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(per_l4_ick_parent_names),
	.ops		= &per_l4_ick_ops,
};

static struct clk gpio2_ick;

static const char *gpio2_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops gpio2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio2_ick_hw = {
	.hw = {
		.clk = &gpio2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO2_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio2_ick = {
	.name		= "gpio2_ick",
	.hw		= &gpio2_ick_hw.hw,
	.parent_names	= gpio2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio2_ick_parent_names),
	.ops		= &gpio2_ick_ops,
};

static struct clk gpio3_dbck;

static const char *gpio3_dbck_parent_names[] = {
	"per_32k_alwon_fck",
};

static const struct clk_ops gpio3_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio3_dbck_hw = {
	.hw = {
		.clk = &gpio3_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio3_dbck = {
	.name		= "gpio3_dbck",
	.hw		= &gpio3_dbck_hw.hw,
	.parent_names	= gpio3_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio3_dbck_parent_names),
	.ops		= &gpio3_dbck_ops,
};

static struct clk gpio3_ick;

static const char *gpio3_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops gpio3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio3_ick_hw = {
	.hw = {
		.clk = &gpio3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio3_ick = {
	.name		= "gpio3_ick",
	.hw		= &gpio3_ick_hw.hw,
	.parent_names	= gpio3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio3_ick_parent_names),
	.ops		= &gpio3_ick_ops,
};

static struct clk gpio4_dbck;

static const char *gpio4_dbck_parent_names[] = {
	"per_32k_alwon_fck",
};

static const struct clk_ops gpio4_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio4_dbck_hw = {
	.hw = {
		.clk = &gpio4_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio4_dbck = {
	.name		= "gpio4_dbck",
	.hw		= &gpio4_dbck_hw.hw,
	.parent_names	= gpio4_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio4_dbck_parent_names),
	.ops		= &gpio4_dbck_ops,
};

static struct clk gpio4_ick;

static const char *gpio4_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops gpio4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio4_ick_hw = {
	.hw = {
		.clk = &gpio4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio4_ick = {
	.name		= "gpio4_ick",
	.hw		= &gpio4_ick_hw.hw,
	.parent_names	= gpio4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio4_ick_parent_names),
	.ops		= &gpio4_ick_ops,
};

static struct clk gpio5_dbck;

static const char *gpio5_dbck_parent_names[] = {
	"per_32k_alwon_fck",
};

static const struct clk_ops gpio5_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio5_dbck_hw = {
	.hw = {
		.clk = &gpio5_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO5_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio5_dbck = {
	.name		= "gpio5_dbck",
	.hw		= &gpio5_dbck_hw.hw,
	.parent_names	= gpio5_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio5_dbck_parent_names),
	.ops		= &gpio5_dbck_ops,
};

static struct clk gpio5_ick;

static const char *gpio5_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops gpio5_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio5_ick_hw = {
	.hw = {
		.clk = &gpio5_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO5_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio5_ick = {
	.name		= "gpio5_ick",
	.hw		= &gpio5_ick_hw.hw,
	.parent_names	= gpio5_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio5_ick_parent_names),
	.ops		= &gpio5_ick_ops,
};

static struct clk gpio6_dbck;

static const char *gpio6_dbck_parent_names[] = {
	"per_32k_alwon_fck",
};

static const struct clk_ops gpio6_dbck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio6_dbck_hw = {
	.hw = {
		.clk = &gpio6_dbck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPIO6_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio6_dbck = {
	.name		= "gpio6_dbck",
	.hw		= &gpio6_dbck_hw.hw,
	.parent_names	= gpio6_dbck_parent_names,
	.num_parents	= ARRAY_SIZE(gpio6_dbck_parent_names),
	.ops		= &gpio6_dbck_ops,
};

static struct clk gpio6_ick;

static const char *gpio6_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops gpio6_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap gpio6_ick_hw = {
	.hw = {
		.clk = &gpio6_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPIO6_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk gpio6_ick = {
	.name		= "gpio6_ick",
	.hw		= &gpio6_ick_hw.hw,
	.parent_names	= gpio6_ick_parent_names,
	.num_parents	= ARRAY_SIZE(gpio6_ick_parent_names),
	.ops		= &gpio6_ick_ops,
};

static struct clk gpmc_fck;

static const char *gpmc_fck_parent_names[] = {
	"core_l3_ick",
};

static const struct clk_ops gpmc_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap gpmc_fck_hw = {
	.hw = {
		.clk = &gpmc_fck,
	},
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

static const struct clksel omap343x_gpt_clksel[] = {
	{ .parent = &omap_32k_fck, .rates = gpt_32k_rates },
	{ .parent = &sys_ck, .rates = gpt_sys_rates },
	{ .parent = NULL },
};

static const char *gpt10_fck_parent_names[] = {
	"omap_32k_fck",
	"sys_ck",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT10_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_GPT10_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_GPT10_SHIFT,
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT11_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_GPT11_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_GPT11_SHIFT,
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

static const char *gpt12_fck_parent_names[] = {
	"secure_32k_fck",
};

static const struct clk_ops gpt12_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap gpt12_fck_hw = {
	.hw = {
		.clk = &gpt12_fck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk gpt12_fck = {
	.name		= "gpt12_fck",
	.hw		= &gpt12_fck_hw.hw,
	.parent_names	= gpt12_fck_parent_names,
	.num_parents	= ARRAY_SIZE(gpt12_fck_parent_names),
	.ops		= &gpt12_fck_ops,
};

static struct clk gpt12_ick;

static const char *gpt12_ick_parent_names[] = {
	"wkup_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT12_SHIFT,
	.clkdm_name	= "wkup_clkdm",
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
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap gpt1_fck_hw = {
	.hw = {
		.clk = &gpt1_fck,
	},
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT1_MASK,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
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
	"wkup_l4_ick",
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
	.enable_bit	= OMAP3430_EN_GPT1_SHIFT,
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT2_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT2_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT2_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT3_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT3_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT3_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT4_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT4_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT4_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT5_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT5_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT5_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT6_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT6_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT6_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT7_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT7_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT7_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT8_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT8_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT8_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	.clksel		= omap343x_gpt_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_GPT9_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_GPT9_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_GPT9_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"core_12m_fck",
};

static const struct clk_ops hdq_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap hdq_fck_hw = {
	.hw = {
		.clk = &hdq_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_HDQ_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_HDQ_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk hdq_ick = {
	.name		= "hdq_ick",
	.hw		= &hdq_ick_hw.hw,
	.parent_names	= hdq_ick_parent_names,
	.num_parents	= ARRAY_SIZE(hdq_ick_parent_names),
	.ops		= &hdq_ick_ops,
};

static struct clk hecc_ck;

static const char *hecc_ck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops hecc_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap hecc_ck_hw = {
	.hw = {
		.clk = &hecc_ck,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_HECC_VBUSP_CLK_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk hecc_ck = {
	.name		= "hecc_ck",
	.hw		= &hecc_ck_hw.hw,
	.parent_names	= hecc_ck_parent_names,
	.num_parents	= ARRAY_SIZE(hecc_ck_parent_names),
	.ops		= &hecc_ck_ops,
};

static struct clk hsotgusb_fck_am35xx;

static const char *hsotgusb_fck_am35xx_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops hsotgusb_fck_am35xx_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap hsotgusb_fck_am35xx_hw = {
	.hw = {
		.clk = &hsotgusb_fck_am35xx,
	},
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_USBOTG_FCLK_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk hsotgusb_fck_am35xx = {
	.name		= "hsotgusb_fck",
	.hw		= &hsotgusb_fck_am35xx_hw.hw,
	.parent_names	= hsotgusb_fck_am35xx_parent_names,
	.num_parents	= ARRAY_SIZE(hsotgusb_fck_am35xx_parent_names),
	.ops		= &hsotgusb_fck_am35xx_ops,
};

static struct clk hsotgusb_ick_3430es1;

static const char *hsotgusb_ick_3430es1_parent_names[] = {
	"core_l3_ick",
};

static const struct clk_ops hsotgusb_ick_3430es1_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap hsotgusb_ick_3430es1_hw = {
	.hw = {
		.clk = &hsotgusb_ick_3430es1,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_HSOTGUSB_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk hsotgusb_ick_3430es1 = {
	.name		= "hsotgusb_ick",
	.hw		= &hsotgusb_ick_3430es1_hw.hw,
	.parent_names	= hsotgusb_ick_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(hsotgusb_ick_3430es1_parent_names),
	.ops		= &hsotgusb_ick_3430es1_ops,
};

static struct clk hsotgusb_ick_3430es2;

static const char *hsotgusb_ick_3430es2_parent_names[] = {
	"core_l3_ick",
};

static const struct clk_ops hsotgusb_ick_3430es2_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap hsotgusb_ick_3430es2_hw = {
	.hw = {
		.clk = &hsotgusb_ick_3430es2,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_HSOTGUSB_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk hsotgusb_ick_3430es2 = {
	.name		= "hsotgusb_ick",
	.hw		= &hsotgusb_ick_3430es2_hw.hw,
	.parent_names	= hsotgusb_ick_3430es2_parent_names,
	.num_parents	= ARRAY_SIZE(hsotgusb_ick_3430es2_parent_names),
	.ops		= &hsotgusb_ick_3430es2_ops,
};

static struct clk hsotgusb_ick_am35xx;

static const char *hsotgusb_ick_am35xx_parent_names[] = {
	"ipss_ick",
};

static const struct clk_ops hsotgusb_ick_am35xx_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap hsotgusb_ick_am35xx_hw = {
	.hw = {
		.clk = &hsotgusb_ick_am35xx,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_USBOTG_VBUSP_CLK_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk hsotgusb_ick_am35xx = {
	.name		= "hsotgusb_ick",
	.hw		= &hsotgusb_ick_am35xx_hw.hw,
	.parent_names	= hsotgusb_ick_am35xx_parent_names,
	.num_parents	= ARRAY_SIZE(hsotgusb_ick_am35xx_parent_names),
	.ops		= &hsotgusb_ick_am35xx_ops,
};

static struct clk i2c1_fck;

static const char *i2c1_fck_parent_names[] = {
	"core_96m_fck",
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
	.enable_bit	= OMAP3430_EN_I2C1_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_I2C1_SHIFT,
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
	"core_96m_fck",
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
	.enable_bit	= OMAP3430_EN_I2C2_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_I2C2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c2_ick = {
	.name		= "i2c2_ick",
	.hw		= &i2c2_ick_hw.hw,
	.parent_names	= i2c2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(i2c2_ick_parent_names),
	.ops		= &i2c2_ick_ops,
};

static struct clk i2c3_fck;

static const char *i2c3_fck_parent_names[] = {
	"core_96m_fck",
};

static const struct clk_ops i2c3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c3_fck_hw = {
	.hw = {
		.clk = &i2c3_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_I2C3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c3_fck = {
	.name		= "i2c3_fck",
	.hw		= &i2c3_fck_hw.hw,
	.parent_names	= i2c3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(i2c3_fck_parent_names),
	.ops		= &i2c3_fck_ops,
};

static struct clk i2c3_ick;

static const char *i2c3_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops i2c3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap i2c3_ick_hw = {
	.hw = {
		.clk = &i2c3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_I2C3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk i2c3_ick = {
	.name		= "i2c3_ick",
	.hw		= &i2c3_ick_hw.hw,
	.parent_names	= i2c3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(i2c3_ick_parent_names),
	.ops		= &i2c3_ick_ops,
};

static struct clk icr_ick;

static const char *icr_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops icr_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap icr_ick_hw = {
	.hw = {
		.clk = &icr_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_ICR_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk icr_ick = {
	.name		= "icr_ick",
	.hw		= &icr_ick_hw.hw,
	.parent_names	= icr_ick_parent_names,
	.num_parents	= ARRAY_SIZE(icr_ick_parent_names),
	.ops		= &icr_ick_ops,
};

static struct clk iva2_ck;

static const char *iva2_ck_parent_names[] = {
	"dpll2_m2_ck",
};

static const struct clk_ops iva2_ck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap iva2_ck_hw = {
	.hw = {
		.clk = &iva2_ck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_IVA2_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_CM_FCLKEN_IVA2_EN_IVA2_SHIFT,
	.clkdm_name	= "iva2_clkdm",
};

static struct clk iva2_ck = {
	.name		= "iva2_ck",
	.hw		= &iva2_ck_hw.hw,
	.parent_names	= iva2_ck_parent_names,
	.num_parents	= ARRAY_SIZE(iva2_ck_parent_names),
	.ops		= &iva2_ck_ops,
};

static struct clk mad2d_ick;

static const char *mad2d_ick_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops mad2d_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mad2d_ick_hw = {
	.hw = {
		.clk = &mad2d_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP3430_EN_MAD2D_SHIFT,
	.clkdm_name	= "d2d_clkdm",
};

static struct clk mad2d_ick = {
	.name		= "mad2d_ick",
	.hw		= &mad2d_ick_hw.hw,
	.parent_names	= mad2d_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mad2d_ick_parent_names),
	.ops		= &mad2d_ick_ops,
};

static struct clk mailboxes_ick;

static const char *mailboxes_ick_parent_names[] = {
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_MAILBOXES_SHIFT,
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
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate common_mcbsp_mcbsp_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel mcbsp_15_clksel[] = {
	{ .parent = &core_96m_fck, .rates = common_mcbsp_96m_rates },
	{ .parent = &mcbsp_clks, .rates = common_mcbsp_mcbsp_rates },
	{ .parent = NULL },
};

static const char *mcbsp1_fck_parent_names[] = {
	"core_96m_fck",
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
	.clksel		= mcbsp_15_clksel,
	.clksel_reg	= OMAP343X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP1_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MCBSP1_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_MCBSP1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp1_ick = {
	.name		= "mcbsp1_ick",
	.hw		= &mcbsp1_ick_hw.hw,
	.parent_names	= mcbsp1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp1_ick_parent_names),
	.ops		= &mcbsp1_ick_ops,
};

static struct clk per_96m_fck;

static const char *per_96m_fck_parent_names[] = {
	"omap_96m_alwon_fck",
};

static const struct clk_ops per_96m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap per_96m_fck_hw = {
	.hw = {
		.clk = &per_96m_fck,
	},
	.clkdm_name	= "per_clkdm",
};

static struct clk per_96m_fck = {
	.name		= "per_96m_fck",
	.hw		= &per_96m_fck_hw.hw,
	.parent_names	= per_96m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(per_96m_fck_parent_names),
	.ops		= &per_96m_fck_ops,
};

static const struct clksel mcbsp_234_clksel[] = {
	{ .parent = &per_96m_fck, .rates = common_mcbsp_96m_rates },
	{ .parent = &mcbsp_clks, .rates = common_mcbsp_mcbsp_rates },
	{ .parent = NULL },
};

static const char *mcbsp2_fck_parent_names[] = {
	"per_96m_fck",
	"mcbsp_clks",
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
	.clksel		= mcbsp_234_clksel,
	.clksel_reg	= OMAP343X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP2_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP2_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp2_fck = {
	.name		= "mcbsp2_fck",
	.hw		= &mcbsp2_fck_hw.hw,
	.parent_names	= mcbsp2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp2_fck_parent_names),
	.ops		= &mcbsp2_fck_ops,
};

static struct clk mcbsp2_ick;

static const char *mcbsp2_ick_parent_names[] = {
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP2_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp2_ick = {
	.name		= "mcbsp2_ick",
	.hw		= &mcbsp2_ick_hw.hw,
	.parent_names	= mcbsp2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp2_ick_parent_names),
	.ops		= &mcbsp2_ick_ops,
};

static struct clk mcbsp3_fck;

static const struct clk_ops mcbsp3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap mcbsp3_fck_hw = {
	.hw = {
		.clk = &mcbsp3_fck,
	},
	.clksel		= mcbsp_234_clksel,
	.clksel_reg	= OMAP343X_CTRL_REGADDR(OMAP343X_CONTROL_DEVCONF1),
	.clksel_mask	= OMAP2_MCBSP3_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp3_fck = {
	.name		= "mcbsp3_fck",
	.hw		= &mcbsp3_fck_hw.hw,
	.parent_names	= mcbsp2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp2_fck_parent_names),
	.ops		= &mcbsp3_fck_ops,
};

static struct clk mcbsp3_ick;

static const char *mcbsp3_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops mcbsp3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcbsp3_ick_hw = {
	.hw = {
		.clk = &mcbsp3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp3_ick = {
	.name		= "mcbsp3_ick",
	.hw		= &mcbsp3_ick_hw.hw,
	.parent_names	= mcbsp3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp3_ick_parent_names),
	.ops		= &mcbsp3_ick_ops,
};

static struct clk mcbsp4_fck;

static const struct clk_ops mcbsp4_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap mcbsp4_fck_hw = {
	.hw = {
		.clk = &mcbsp4_fck,
	},
	.clksel		= mcbsp_234_clksel,
	.clksel_reg	= OMAP343X_CTRL_REGADDR(OMAP343X_CONTROL_DEVCONF1),
	.clksel_mask	= OMAP2_MCBSP4_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp4_fck = {
	.name		= "mcbsp4_fck",
	.hw		= &mcbsp4_fck_hw.hw,
	.parent_names	= mcbsp2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp2_fck_parent_names),
	.ops		= &mcbsp4_fck_ops,
};

static struct clk mcbsp4_ick;

static const char *mcbsp4_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops mcbsp4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcbsp4_ick_hw = {
	.hw = {
		.clk = &mcbsp4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_MCBSP4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk mcbsp4_ick = {
	.name		= "mcbsp4_ick",
	.hw		= &mcbsp4_ick_hw.hw,
	.parent_names	= mcbsp4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp4_ick_parent_names),
	.ops		= &mcbsp4_ick_ops,
};

static struct clk mcbsp5_fck;

static const struct clk_ops mcbsp5_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap mcbsp5_fck_hw = {
	.hw = {
		.clk = &mcbsp5_fck,
	},
	.clksel		= mcbsp_15_clksel,
	.clksel_reg	= OMAP343X_CTRL_REGADDR(OMAP343X_CONTROL_DEVCONF1),
	.clksel_mask	= OMAP2_MCBSP5_CLKS_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MCBSP5_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp5_fck = {
	.name		= "mcbsp5_fck",
	.hw		= &mcbsp5_fck_hw.hw,
	.parent_names	= mcbsp1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp1_fck_parent_names),
	.ops		= &mcbsp5_fck_ops,
};

static struct clk mcbsp5_ick;

static const char *mcbsp5_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mcbsp5_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcbsp5_ick_hw = {
	.hw = {
		.clk = &mcbsp5_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_MCBSP5_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcbsp5_ick = {
	.name		= "mcbsp5_ick",
	.hw		= &mcbsp5_ick_hw.hw,
	.parent_names	= mcbsp5_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcbsp5_ick_parent_names),
	.ops		= &mcbsp5_ick_ops,
};

static struct clk mcspi1_fck;

static const char *mcspi1_fck_parent_names[] = {
	"core_48m_fck",
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
	.enable_bit	= OMAP3430_EN_MCSPI1_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_MCSPI1_SHIFT,
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
	"core_48m_fck",
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
	.enable_bit	= OMAP3430_EN_MCSPI2_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_MCSPI2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi2_ick = {
	.name		= "mcspi2_ick",
	.hw		= &mcspi2_ick_hw.hw,
	.parent_names	= mcspi2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi2_ick_parent_names),
	.ops		= &mcspi2_ick_ops,
};

static struct clk mcspi3_fck;

static const char *mcspi3_fck_parent_names[] = {
	"core_48m_fck",
};

static const struct clk_ops mcspi3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi3_fck_hw = {
	.hw = {
		.clk = &mcspi3_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MCSPI3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi3_fck = {
	.name		= "mcspi3_fck",
	.hw		= &mcspi3_fck_hw.hw,
	.parent_names	= mcspi3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi3_fck_parent_names),
	.ops		= &mcspi3_fck_ops,
};

static struct clk mcspi3_ick;

static const char *mcspi3_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mcspi3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi3_ick_hw = {
	.hw = {
		.clk = &mcspi3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_MCSPI3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi3_ick = {
	.name		= "mcspi3_ick",
	.hw		= &mcspi3_ick_hw.hw,
	.parent_names	= mcspi3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi3_ick_parent_names),
	.ops		= &mcspi3_ick_ops,
};

static struct clk mcspi4_fck;

static const char *mcspi4_fck_parent_names[] = {
	"core_48m_fck",
};

static const struct clk_ops mcspi4_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi4_fck_hw = {
	.hw = {
		.clk = &mcspi4_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MCSPI4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi4_fck = {
	.name		= "mcspi4_fck",
	.hw		= &mcspi4_fck_hw.hw,
	.parent_names	= mcspi4_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi4_fck_parent_names),
	.ops		= &mcspi4_fck_ops,
};

static struct clk mcspi4_ick;

static const char *mcspi4_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mcspi4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mcspi4_ick_hw = {
	.hw = {
		.clk = &mcspi4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_MCSPI4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mcspi4_ick = {
	.name		= "mcspi4_ick",
	.hw		= &mcspi4_ick_hw.hw,
	.parent_names	= mcspi4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mcspi4_ick_parent_names),
	.ops		= &mcspi4_ick_ops,
};

static struct clk mmchs1_fck;

static const char *mmchs1_fck_parent_names[] = {
	"core_96m_fck",
};

static const struct clk_ops mmchs1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs1_fck_hw = {
	.hw = {
		.clk = &mmchs1_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MMC1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs1_fck = {
	.name		= "mmchs1_fck",
	.hw		= &mmchs1_fck_hw.hw,
	.parent_names	= mmchs1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs1_fck_parent_names),
	.ops		= &mmchs1_fck_ops,
};

static struct clk mmchs1_ick;

static const char *mmchs1_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mmchs1_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs1_ick_hw = {
	.hw = {
		.clk = &mmchs1_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_MMC1_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs1_ick = {
	.name		= "mmchs1_ick",
	.hw		= &mmchs1_ick_hw.hw,
	.parent_names	= mmchs1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs1_ick_parent_names),
	.ops		= &mmchs1_ick_ops,
};

static struct clk mmchs2_fck;

static const char *mmchs2_fck_parent_names[] = {
	"core_96m_fck",
};

static const struct clk_ops mmchs2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs2_fck_hw = {
	.hw = {
		.clk = &mmchs2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MMC2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs2_fck = {
	.name		= "mmchs2_fck",
	.hw		= &mmchs2_fck_hw.hw,
	.parent_names	= mmchs2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs2_fck_parent_names),
	.ops		= &mmchs2_fck_ops,
};

static struct clk mmchs2_ick;

static const char *mmchs2_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mmchs2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs2_ick_hw = {
	.hw = {
		.clk = &mmchs2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_MMC2_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs2_ick = {
	.name		= "mmchs2_ick",
	.hw		= &mmchs2_ick_hw.hw,
	.parent_names	= mmchs2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs2_ick_parent_names),
	.ops		= &mmchs2_ick_ops,
};

static struct clk mmchs3_fck;

static const char *mmchs3_fck_parent_names[] = {
	"core_96m_fck",
};

static const struct clk_ops mmchs3_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs3_fck_hw = {
	.hw = {
		.clk = &mmchs3_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430ES2_EN_MMC3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs3_fck = {
	.name		= "mmchs3_fck",
	.hw		= &mmchs3_fck_hw.hw,
	.parent_names	= mmchs3_fck_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs3_fck_parent_names),
	.ops		= &mmchs3_fck_ops,
};

static struct clk mmchs3_ick;

static const char *mmchs3_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops mmchs3_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap mmchs3_ick_hw = {
	.hw = {
		.clk = &mmchs3_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430ES2_EN_MMC3_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mmchs3_ick = {
	.name		= "mmchs3_ick",
	.hw		= &mmchs3_ick_hw.hw,
	.parent_names	= mmchs3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mmchs3_ick_parent_names),
	.ops		= &mmchs3_ick_ops,
};

static struct clk modem_fck;

static const char *modem_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops modem_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap modem_fck_hw = {
	.hw = {
		.clk = &modem_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_MODEM_SHIFT,
	.clkdm_name	= "d2d_clkdm",
};

static struct clk modem_fck = {
	.name		= "modem_fck",
	.hw		= &modem_fck_hw.hw,
	.parent_names	= modem_fck_parent_names,
	.num_parents	= ARRAY_SIZE(modem_fck_parent_names),
	.ops		= &modem_fck_ops,
};

static struct clk mspro_fck;

static const char *mspro_fck_parent_names[] = {
	"core_96m_fck",
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
	.enable_bit	= OMAP3430_EN_MSPRO_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_MSPRO_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk mspro_ick = {
	.name		= "mspro_ick",
	.hw		= &mspro_ick_hw.hw,
	.parent_names	= mspro_ick_parent_names,
	.num_parents	= ARRAY_SIZE(mspro_ick_parent_names),
	.ops		= &mspro_ick_ops,
};

static struct clk omap_192m_alwon_fck;

static const char *omap_192m_alwon_fck_parent_names[] = {
	"dpll4_m2x2_ck",
};

static const struct clk_ops omap_192m_alwon_fck_ops = {
};

static struct clk_hw_omap omap_192m_alwon_fck_hw = {
	.hw = {
		.clk = &omap_192m_alwon_fck,
	},
};

static struct clk omap_192m_alwon_fck = {
	.name		= "omap_192m_alwon_fck",
	.hw		= &omap_192m_alwon_fck_hw.hw,
	.parent_names	= omap_192m_alwon_fck_parent_names,
	.num_parents	= ARRAY_SIZE(omap_192m_alwon_fck_parent_names),
	.ops		= &omap_192m_alwon_fck_ops,
};

static struct clk omap_32ksync_ick;

static const char *omap_32ksync_ick_parent_names[] = {
	"wkup_l4_ick",
};

static const struct clk_ops omap_32ksync_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap omap_32ksync_ick_hw = {
	.hw = {
		.clk = &omap_32ksync_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_32KSYNC_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk omap_32ksync_ick = {
	.name		= "omap_32ksync_ick",
	.hw		= &omap_32ksync_ick_hw.hw,
	.parent_names	= omap_32ksync_ick_parent_names,
	.num_parents	= ARRAY_SIZE(omap_32ksync_ick_parent_names),
	.ops		= &omap_32ksync_ick_ops,
};

static const struct clksel_rate omap_96m_alwon_fck_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_36XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_36XX },
	{ .div = 0 }
};

static const struct clksel omap_96m_alwon_fck_clksel[] = {
	{ .parent = &omap_192m_alwon_fck, .rates = omap_96m_alwon_fck_rates },
	{ .parent = NULL },
};

static const char *omap_96m_alwon_fck_3630_parent_names[] = {
	"omap_192m_alwon_fck",
};

static struct clk omap_96m_alwon_fck_3630;

static const struct clk_ops omap_96m_alwon_fck_3630_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap omap_96m_alwon_fck_3630_hw = {
	.hw = {
		.clk = &omap_96m_alwon_fck_3630,
	},
	.clksel		= omap_96m_alwon_fck_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3630_CLKSEL_96M_MASK,
};

static struct clk omap_96m_alwon_fck_3630 = {
	.name		= "omap_96m_alwon_fck",
	.hw		= &omap_96m_alwon_fck_3630_hw.hw,
	.parent_names	= omap_96m_alwon_fck_3630_parent_names,
	.num_parents	= ARRAY_SIZE(omap_96m_alwon_fck_3630_parent_names),
	.ops		= &omap_96m_alwon_fck_3630_ops,
};

static struct clk omapctrl_ick;

static const char *omapctrl_ick_parent_names[] = {
	"core_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_OMAPCTRL_SHIFT,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk omapctrl_ick = {
	.name		= "omapctrl_ick",
	.hw		= &omapctrl_ick_hw.hw,
	.parent_names	= omapctrl_ick_parent_names,
	.num_parents	= ARRAY_SIZE(omapctrl_ick_parent_names),
	.ops		= &omapctrl_ick_ops,
};

static const struct clksel_rate pclk_emu_rates[] = {
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 6, .val = 6, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel pclk_emu_clksel[] = {
	{ .parent = &emu_src_ck, .rates = pclk_emu_rates },
	{ .parent = NULL },
};

static const char *pclk_fck_parent_names[] = {
	"emu_src_ck",
};

static struct clk pclk_fck;

static const struct clk_ops pclk_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap pclk_fck_hw = {
	.hw = {
		.clk = &pclk_fck,
	},
	.clksel		= pclk_emu_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_CLKSEL_PCLK_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk pclk_fck = {
	.name		= "pclk_fck",
	.hw		= &pclk_fck_hw.hw,
	.parent_names	= pclk_fck_parent_names,
	.num_parents	= ARRAY_SIZE(pclk_fck_parent_names),
	.ops		= &pclk_fck_ops,
};

static const struct clksel_rate pclkx2_emu_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel pclkx2_emu_clksel[] = {
	{ .parent = &emu_src_ck, .rates = pclkx2_emu_rates },
	{ .parent = NULL },
};

static const char *pclkx2_fck_parent_names[] = {
	"emu_src_ck",
};

static struct clk pclkx2_fck;

static const struct clk_ops pclkx2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap pclkx2_fck_hw = {
	.hw = {
		.clk = &pclkx2_fck,
	},
	.clksel		= pclkx2_emu_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_CLKSEL_PCLKX2_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk pclkx2_fck = {
	.name		= "pclkx2_fck",
	.hw		= &pclkx2_fck_hw.hw,
	.parent_names	= pclkx2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(pclkx2_fck_parent_names),
	.ops		= &pclkx2_fck_ops,
};

static struct clk per_48m_fck;

static const char *per_48m_fck_parent_names[] = {
	"omap_48m_fck",
};

static const struct clk_ops per_48m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap per_48m_fck_hw = {
	.hw = {
		.clk = &per_48m_fck,
	},
	.clkdm_name	= "per_clkdm",
};

static struct clk per_48m_fck = {
	.name		= "per_48m_fck",
	.hw		= &per_48m_fck_hw.hw,
	.parent_names	= per_48m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(per_48m_fck_parent_names),
	.ops		= &per_48m_fck_ops,
};

static struct clk security_l3_ick;

static const char *security_l3_ick_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops security_l3_ick_ops = {
};

static struct clk_hw_omap security_l3_ick_hw = {
	.hw = {
		.clk = &security_l3_ick,
	},
};

static struct clk security_l3_ick = {
	.name		= "security_l3_ick",
	.hw		= &security_l3_ick_hw.hw,
	.parent_names	= security_l3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(security_l3_ick_parent_names),
	.ops		= &security_l3_ick_ops,
};

static struct clk pka_ick;

static const char *pka_ick_parent_names[] = {
	"security_l3_ick",
};

static const struct clk_ops pka_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap pka_ick_hw = {
	.hw = {
		.clk = &pka_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP3430_EN_PKA_SHIFT,
};

static struct clk pka_ick = {
	.name		= "pka_ick",
	.hw		= &pka_ick_hw.hw,
	.parent_names	= pka_ick_parent_names,
	.num_parents	= ARRAY_SIZE(pka_ick_parent_names),
	.ops		= &pka_ick_ops,
};

static const struct clksel div2_l4_clksel[] = {
	{ .parent = &l4_ick, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *rm_ick_parent_names[] = {
	"l4_ick",
};

static struct clk rm_ick;

static const struct clk_ops rm_ick_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap rm_ick_hw = {
	.hw = {
		.clk = &rm_ick,
	},
	.clksel		= div2_l4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_RM_MASK,
};

static struct clk rm_ick = {
	.name		= "rm_ick",
	.hw		= &rm_ick_hw.hw,
	.parent_names	= rm_ick_parent_names,
	.num_parents	= ARRAY_SIZE(rm_ick_parent_names),
	.ops		= &rm_ick_ops,
};

static struct clk rng_ick;

static const char *rng_ick_parent_names[] = {
	"security_l4_ick2",
};

static const struct clk_ops rng_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap rng_ick_hw = {
	.hw = {
		.clk = &rng_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP3430_EN_RNG_SHIFT,
};

static struct clk rng_ick = {
	.name		= "rng_ick",
	.hw		= &rng_ick_hw.hw,
	.parent_names	= rng_ick_parent_names,
	.num_parents	= ARRAY_SIZE(rng_ick_parent_names),
	.ops		= &rng_ick_ops,
};

static struct clk sad2d_ick;

static const char *sad2d_ick_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops sad2d_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sad2d_ick_hw = {
	.hw = {
		.clk = &sad2d_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_SAD2D_SHIFT,
	.clkdm_name	= "d2d_clkdm",
};

static struct clk sad2d_ick = {
	.name		= "sad2d_ick",
	.hw		= &sad2d_ick_hw.hw,
	.parent_names	= sad2d_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sad2d_ick_parent_names),
	.ops		= &sad2d_ick_ops,
};

static struct clk sdrc_ick;

static const char *sdrc_ick_parent_names[] = {
	"core_l3_ick",
};

static const struct clk_ops sdrc_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sdrc_ick_hw = {
	.hw = {
		.clk = &sdrc_ick,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_SDRC_SHIFT,
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

static const struct clksel_rate sgx_core_rates[] = {
	{ .div = 2, .val = 5, .flags = RATE_IN_36XX },
	{ .div = 3, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 6, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate sgx_96m_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate sgx_192m_rates[] = {
	{ .div = 1, .val = 4, .flags = RATE_IN_36XX },
	{ .div = 0 }
};

static const struct clksel_rate sgx_corex2_rates[] = {
	{ .div = 3, .val = 6, .flags = RATE_IN_36XX },
	{ .div = 5, .val = 7, .flags = RATE_IN_36XX },
	{ .div = 0 }
};

static const struct clksel sgx_clksel[] = {
	{ .parent = &core_ck, .rates = sgx_core_rates },
	{ .parent = &cm_96m_fck, .rates = sgx_96m_rates },
	{ .parent = &omap_192m_alwon_fck, .rates = sgx_192m_rates },
	{ .parent = &corex2_fck, .rates = sgx_corex2_rates },
	{ .parent = NULL },
};

static const char *sgx_fck_parent_names[] = {
	"core_ck",
	"cm_96m_fck",
	"omap_192m_alwon_fck",
	"corex2_fck",
};

static struct clk sgx_fck;

static const struct clk_ops sgx_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap sgx_fck_hw = {
	.hw = {
		.clk = &sgx_fck,
	},
	.clksel		= sgx_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430ES2_SGX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430ES2_CLKSEL_SGX_MASK,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430ES2_SGX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES2_CM_FCLKEN_SGX_EN_SGX_SHIFT,
	.clkdm_name	= "sgx_clkdm",
};

static struct clk sgx_fck = {
	.name		= "sgx_fck",
	.hw		= &sgx_fck_hw.hw,
	.parent_names	= sgx_fck_parent_names,
	.num_parents	= ARRAY_SIZE(sgx_fck_parent_names),
	.ops		= &sgx_fck_ops,
};

static struct clk sgx_ick;

static const char *sgx_ick_parent_names[] = {
	"l3_ick",
};

static const struct clk_ops sgx_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sgx_ick_hw = {
	.hw = {
		.clk = &sgx_ick,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430ES2_SGX_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430ES2_CM_ICLKEN_SGX_EN_SGX_SHIFT,
	.clkdm_name	= "sgx_clkdm",
};

static struct clk sgx_ick = {
	.name		= "sgx_ick",
	.hw		= &sgx_ick_hw.hw,
	.parent_names	= sgx_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sgx_ick_parent_names),
	.ops		= &sgx_ick_ops,
};

static struct clk sha11_ick;

static const char *sha11_ick_parent_names[] = {
	"security_l4_ick2",
};

static const struct clk_ops sha11_ick_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sha11_ick_hw = {
	.hw = {
		.clk = &sha11_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP3430_EN_SHA11_SHIFT,
};

static struct clk sha11_ick = {
	.name		= "sha11_ick",
	.hw		= &sha11_ick_hw.hw,
	.parent_names	= sha11_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sha11_ick_parent_names),
	.ops		= &sha11_ick_ops,
};

static struct clk sha12_ick;

static const char *sha12_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops sha12_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sha12_ick_hw = {
	.hw = {
		.clk = &sha12_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_SHA12_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk sha12_ick = {
	.name		= "sha12_ick",
	.hw		= &sha12_ick_hw.hw,
	.parent_names	= sha12_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sha12_ick_parent_names),
	.ops		= &sha12_ick_ops,
};

static struct clk sr1_fck;

static const char *sr1_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops sr1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sr1_fck_hw = {
	.hw = {
		.clk = &sr1_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_SR1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sr1_fck = {
	.name		= "sr1_fck",
	.hw		= &sr1_fck_hw.hw,
	.parent_names	= sr1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(sr1_fck_parent_names),
	.ops		= &sr1_fck_ops,
};

static struct clk sr2_fck;

static const char *sr2_fck_parent_names[] = {
	"sys_ck",
};

static const struct clk_ops sr2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sr2_fck_hw = {
	.hw = {
		.clk = &sr2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_SR2_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk sr2_fck = {
	.name		= "sr2_fck",
	.hw		= &sr2_fck_hw.hw,
	.parent_names	= sr2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(sr2_fck_parent_names),
	.ops		= &sr2_fck_ops,
};

static struct clk sr_l4_ick;

static const char *sr_l4_ick_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops sr_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap sr_l4_ick_hw = {
	.hw = {
		.clk = &sr_l4_ick,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk sr_l4_ick = {
	.name		= "sr_l4_ick",
	.hw		= &sr_l4_ick_hw.hw,
	.parent_names	= sr_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(sr_l4_ick_parent_names),
	.ops		= &sr_l4_ick_ops,
};

static struct clk ssi_l4_ick;

static const char *ssi_l4_ick_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops ssi_l4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap ssi_l4_ick_hw = {
	.hw = {
		.clk = &ssi_l4_ick,
	},
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_l4_ick = {
	.name		= "ssi_l4_ick",
	.hw		= &ssi_l4_ick_hw.hw,
	.parent_names	= ssi_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_l4_ick_parent_names),
	.ops		= &ssi_l4_ick_ops,
};

static struct clk ssi_ick_3430es1;

static const char *ssi_ick_3430es1_parent_names[] = {
	"ssi_l4_ick",
};

static const struct clk_ops ssi_ick_3430es1_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap ssi_ick_3430es1_hw = {
	.hw = {
		.clk = &ssi_ick_3430es1,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_SSI_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_ick_3430es1 = {
	.name		= "ssi_ick",
	.hw		= &ssi_ick_3430es1_hw.hw,
	.parent_names	= ssi_ick_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_ick_3430es1_parent_names),
	.ops		= &ssi_ick_3430es1_ops,
};

static struct clk ssi_ick_3430es2;

static const char *ssi_ick_3430es2_parent_names[] = {
	"ssi_l4_ick",
};

static const struct clk_ops ssi_ick_3430es2_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap ssi_ick_3430es2_hw = {
	.hw = {
		.clk = &ssi_ick_3430es2,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430_EN_SSI_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_ick_3430es2 = {
	.name		= "ssi_ick",
	.hw		= &ssi_ick_3430es2_hw.hw,
	.parent_names	= ssi_ick_3430es2_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_ick_3430es2_parent_names),
	.ops		= &ssi_ick_3430es2_ops,
};

static const struct clksel_rate ssi_ssr_corex2_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 3, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 6, .val = 6, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 8, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel ssi_ssr_clksel[] = {
	{ .parent = &corex2_fck, .rates = ssi_ssr_corex2_rates },
	{ .parent = NULL },
};

static const char *ssi_ssr_fck_3430es1_parent_names[] = {
	"corex2_fck",
};

static struct clk ssi_ssr_fck_3430es1;

static const struct clk_ops ssi_ssr_fck_3430es1_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap ssi_ssr_fck_3430es1_hw = {
	.hw = {
		.clk = &ssi_ssr_fck_3430es1,
	},
	.clksel		= ssi_ssr_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_SSI_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_SSI_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_ssr_fck_3430es1 = {
	.name		= "ssi_ssr_fck",
	.hw		= &ssi_ssr_fck_3430es1_hw.hw,
	.parent_names	= ssi_ssr_fck_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_ssr_fck_3430es1_parent_names),
	.ops		= &ssi_ssr_fck_3430es1_ops,
};

static struct clk ssi_ssr_fck_3430es2;

static const struct clk_ops ssi_ssr_fck_3430es2_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap ssi_ssr_fck_3430es2_hw = {
	.hw = {
		.clk = &ssi_ssr_fck_3430es2,
	},
	.clksel		= ssi_ssr_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430_CLKSEL_SSI_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_SSI_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ssi_ssr_fck_3430es2 = {
	.name		= "ssi_ssr_fck",
	.hw		= &ssi_ssr_fck_3430es2_hw.hw,
	.parent_names	= ssi_ssr_fck_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_ssr_fck_3430es1_parent_names),
	.ops		= &ssi_ssr_fck_3430es2_ops,
};

static struct clk ssi_sst_fck_3430es1;

static const char *ssi_sst_fck_3430es1_parent_names[] = {
	"ssi_ssr_fck_3430es1",
};

static const struct clk_ops ssi_sst_fck_3430es1_ops = {
	.recalc_rate	= &omap_fixed_divisor_recalc,
};

static struct clk_hw_omap ssi_sst_fck_3430es1_hw = {
	.hw = {
		.clk = &ssi_sst_fck_3430es1,
	},
	.fixed_div	= 2,
};

static struct clk ssi_sst_fck_3430es1 = {
	.name		= "ssi_sst_fck",
	.hw		= &ssi_sst_fck_3430es1_hw.hw,
	.parent_names	= ssi_sst_fck_3430es1_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_sst_fck_3430es1_parent_names),
	.ops		= &ssi_sst_fck_3430es1_ops,
};

static struct clk ssi_sst_fck_3430es2;

static const char *ssi_sst_fck_3430es2_parent_names[] = {
	"ssi_ssr_fck_3430es2",
};

static const struct clk_ops ssi_sst_fck_3430es2_ops = {
	.recalc_rate	= &omap_fixed_divisor_recalc,
};

static struct clk_hw_omap ssi_sst_fck_3430es2_hw = {
	.hw = {
		.clk = &ssi_sst_fck_3430es2,
	},
	.fixed_div	= 2,
};

static struct clk ssi_sst_fck_3430es2 = {
	.name		= "ssi_sst_fck",
	.hw		= &ssi_sst_fck_3430es2_hw.hw,
	.parent_names	= ssi_sst_fck_3430es2_parent_names,
	.num_parents	= ARRAY_SIZE(ssi_sst_fck_3430es2_parent_names),
	.ops		= &ssi_sst_fck_3430es2_ops,
};

static struct clk sys_clkout1;

static const char *sys_clkout1_parent_names[] = {
	"osc_sys_ck",
};

static const struct clk_ops sys_clkout1_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap sys_clkout1_hw = {
	.hw = {
		.clk = &sys_clkout1,
	},
	.enable_reg	= OMAP3430_PRM_CLKOUT_CTRL,
	.enable_bit	= OMAP3430_CLKOUT_EN_SHIFT,
};

static struct clk sys_clkout1 = {
	.name		= "sys_clkout1",
	.hw		= &sys_clkout1_hw.hw,
	.parent_names	= sys_clkout1_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout1_parent_names),
	.ops		= &sys_clkout1_ops,
};

static const struct clksel_rate sys_clkout2_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 16, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel sys_clkout2_clksel[] = {
	{ .parent = &clkout2_src_ck, .rates = sys_clkout2_rates },
	{ .parent = NULL },
};

static const char *sys_clkout2_parent_names[] = {
	"clkout2_src_ck",
};

static struct clk sys_clkout2;

static const struct clk_ops sys_clkout2_ops = {
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap sys_clkout2_hw = {
	.hw = {
		.clk = &sys_clkout2,
	},
	.clksel		= sys_clkout2_clksel,
	.clksel_reg	= OMAP3430_CM_CLKOUT_CTRL,
	.clksel_mask	= OMAP3430_CLKOUT2_DIV_MASK,
};

static struct clk sys_clkout2 = {
	.name		= "sys_clkout2",
	.hw		= &sys_clkout2_hw.hw,
	.parent_names	= sys_clkout2_parent_names,
	.num_parents	= ARRAY_SIZE(sys_clkout2_parent_names),
	.ops		= &sys_clkout2_ops,
};

static const struct clksel_rate traceclk_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_3XXX },
	{ .div = 2, .val = 2, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static struct clk traceclk_src_fck;

static const struct clk_ops traceclk_src_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap traceclk_src_fck_hw = {
	.hw = {
		.clk = &traceclk_src_fck,
	},
	.clksel		= emu_src_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_TRACE_MUX_CTRL_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk traceclk_src_fck = {
	.name		= "traceclk_src_fck",
	.hw		= &traceclk_src_fck_hw.hw,
	.parent_names	= emu_src_ck_parent_names,
	.num_parents	= ARRAY_SIZE(emu_src_ck_parent_names),
	.ops		= &traceclk_src_fck_ops,
};

static const struct clksel traceclk_clksel[] = {
	{ .parent = &traceclk_src_fck, .rates = traceclk_rates },
	{ .parent = NULL },
};

static const char *traceclk_fck_parent_names[] = {
	"traceclk_src_fck",
};

static struct clk traceclk_fck;

static const struct clk_ops traceclk_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.recalc_rate	= &omap2_clksel_recalc,
	.set_rate	= &omap2_clksel_set_rate,
	.round_rate	= &omap2_clksel_round_rate,
};

static struct clk_hw_omap traceclk_fck_hw = {
	.hw = {
		.clk = &traceclk_fck,
	},
	.clksel		= traceclk_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP3430_EMU_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP3430_CLKSEL_TRACECLK_MASK,
	.clkdm_name	= "emu_clkdm",
};

static struct clk traceclk_fck = {
	.name		= "traceclk_fck",
	.hw		= &traceclk_fck_hw.hw,
	.parent_names	= traceclk_fck_parent_names,
	.num_parents	= ARRAY_SIZE(traceclk_fck_parent_names),
	.ops		= &traceclk_fck_ops,
};

static struct clk ts_fck;

static const char *ts_fck_parent_names[] = {
	"omap_32k_fck",
};

static const struct clk_ops ts_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap ts_fck_hw = {
	.hw = {
		.clk = &ts_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP3430ES2_CM_FCLKEN3),
	.enable_bit	= OMAP3430ES2_EN_TS_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk ts_fck = {
	.name		= "ts_fck",
	.hw		= &ts_fck_hw.hw,
	.parent_names	= ts_fck_parent_names,
	.num_parents	= ARRAY_SIZE(ts_fck_parent_names),
	.ops		= &ts_fck_ops,
};

static struct clk uart1_fck;

static const char *uart1_fck_parent_names[] = {
	"core_48m_fck",
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
	.enable_bit	= OMAP3430_EN_UART1_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_UART1_SHIFT,
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
	"core_48m_fck",
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
	.enable_bit	= OMAP3430_EN_UART2_SHIFT,
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
	"core_l4_ick",
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
	.enable_bit	= OMAP3430_EN_UART2_SHIFT,
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
	"per_48m_fck",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_UART3_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_UART3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk uart3_ick = {
	.name		= "uart3_ick",
	.hw		= &uart3_ick_hw.hw,
	.parent_names	= uart3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(uart3_ick_parent_names),
	.ops		= &uart3_ick_ops,
};

static struct clk uart4_fck;

static const char *uart4_fck_parent_names[] = {
	"per_48m_fck",
};

static const struct clk_ops uart4_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart4_fck_hw = {
	.hw = {
		.clk = &uart4_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3630_EN_UART4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk uart4_fck = {
	.name		= "uart4_fck",
	.hw		= &uart4_fck_hw.hw,
	.parent_names	= uart4_fck_parent_names,
	.num_parents	= ARRAY_SIZE(uart4_fck_parent_names),
	.ops		= &uart4_fck_ops,
};

static struct clk uart4_fck_am35xx;

static const char *uart4_fck_am35xx_parent_names[] = {
	"per_48m_fck",
};

static const struct clk_ops uart4_fck_am35xx_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart4_fck_am35xx_hw = {
	.hw = {
		.clk = &uart4_fck_am35xx,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP3430_EN_UART4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart4_fck_am35xx = {
	.name		= "uart4_fck",
	.hw		= &uart4_fck_am35xx_hw.hw,
	.parent_names	= uart4_fck_am35xx_parent_names,
	.num_parents	= ARRAY_SIZE(uart4_fck_am35xx_parent_names),
	.ops		= &uart4_fck_am35xx_ops,
};

static struct clk uart4_ick;

static const char *uart4_ick_parent_names[] = {
	"per_l4_ick",
};

static const struct clk_ops uart4_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart4_ick_hw = {
	.hw = {
		.clk = &uart4_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3630_EN_UART4_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk uart4_ick = {
	.name		= "uart4_ick",
	.hw		= &uart4_ick_hw.hw,
	.parent_names	= uart4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(uart4_ick_parent_names),
	.ops		= &uart4_ick_ops,
};

static struct clk uart4_ick_am35xx;

static const char *uart4_ick_am35xx_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops uart4_ick_am35xx_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap uart4_ick_am35xx_hw = {
	.hw = {
		.clk = &uart4_ick_am35xx,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= AM35XX_EN_UART4_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk uart4_ick_am35xx = {
	.name		= "uart4_ick",
	.hw		= &uart4_ick_am35xx_hw.hw,
	.parent_names	= uart4_ick_am35xx_parent_names,
	.num_parents	= ARRAY_SIZE(uart4_ick_am35xx_parent_names),
	.ops		= &uart4_ick_am35xx_ops,
};

static const struct clksel usb_l4_clksel[] = {
	{ .parent = &l4_ick, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *usb_l4_ick_parent_names[] = {
	"l4_ick",
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
	.clksel		= usb_l4_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430ES1_CLKSEL_FSHOSTUSB_MASK,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP3430ES1_EN_FSHOSTUSB_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk usb_l4_ick = {
	.name		= "usb_l4_ick",
	.hw		= &usb_l4_ick_hw.hw,
	.parent_names	= usb_l4_ick_parent_names,
	.num_parents	= ARRAY_SIZE(usb_l4_ick_parent_names),
	.ops		= &usb_l4_ick_ops,
};

static struct clk usbhost_120m_fck;

static const char *usbhost_120m_fck_parent_names[] = {
	"dpll5_m2_ck",
};

static const struct clk_ops usbhost_120m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap usbhost_120m_fck_hw = {
	.hw = {
		.clk = &usbhost_120m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430ES2_USBHOST_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES2_EN_USBHOST2_SHIFT,
	.clkdm_name	= "usbhost_clkdm",
};

static struct clk usbhost_120m_fck = {
	.name		= "usbhost_120m_fck",
	.hw		= &usbhost_120m_fck_hw.hw,
	.parent_names	= usbhost_120m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(usbhost_120m_fck_parent_names),
	.ops		= &usbhost_120m_fck_ops,
};

static struct clk usbhost_48m_fck;

static const char *usbhost_48m_fck_parent_names[] = {
	"omap_48m_fck",
};

static const struct clk_ops usbhost_48m_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap usbhost_48m_fck_hw = {
	.hw = {
		.clk = &usbhost_48m_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430ES2_USBHOST_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES2_EN_USBHOST1_SHIFT,
	.clkdm_name	= "usbhost_clkdm",
};

static struct clk usbhost_48m_fck = {
	.name		= "usbhost_48m_fck",
	.hw		= &usbhost_48m_fck_hw.hw,
	.parent_names	= usbhost_48m_fck_parent_names,
	.num_parents	= ARRAY_SIZE(usbhost_48m_fck_parent_names),
	.ops		= &usbhost_48m_fck_ops,
};

static struct clk usbhost_ick;

static const char *usbhost_ick_parent_names[] = {
	"l4_ick",
};

static const struct clk_ops usbhost_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap usbhost_ick_hw = {
	.hw = {
		.clk = &usbhost_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430ES2_USBHOST_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430ES2_EN_USBHOST_SHIFT,
	.clkdm_name	= "usbhost_clkdm",
};

static struct clk usbhost_ick = {
	.name		= "usbhost_ick",
	.hw		= &usbhost_ick_hw.hw,
	.parent_names	= usbhost_ick_parent_names,
	.num_parents	= ARRAY_SIZE(usbhost_ick_parent_names),
	.ops		= &usbhost_ick_ops,
};

static struct clk usbtll_fck;

static const char *usbtll_fck_parent_names[] = {
	"dpll5_m2_ck",
};

static const struct clk_ops usbtll_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap usbtll_fck_hw = {
	.hw = {
		.clk = &usbtll_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP3430ES2_CM_FCLKEN3),
	.enable_bit	= OMAP3430ES2_EN_USBTLL_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk usbtll_fck = {
	.name		= "usbtll_fck",
	.hw		= &usbtll_fck_hw.hw,
	.parent_names	= usbtll_fck_parent_names,
	.num_parents	= ARRAY_SIZE(usbtll_fck_parent_names),
	.ops		= &usbtll_fck_ops,
};

static struct clk usbtll_ick;

static const char *usbtll_ick_parent_names[] = {
	"core_l4_ick",
};

static const struct clk_ops usbtll_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap usbtll_ick_hw = {
	.hw = {
		.clk = &usbtll_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP3430ES2_EN_USBTLL_SHIFT,
	.clkdm_name	= "core_l4_clkdm",
};

static struct clk usbtll_ick = {
	.name		= "usbtll_ick",
	.hw		= &usbtll_ick_hw.hw,
	.parent_names	= usbtll_ick_parent_names,
	.num_parents	= ARRAY_SIZE(usbtll_ick_parent_names),
	.ops		= &usbtll_ick_ops,
};

static const struct clksel_rate usim_96m_rates[] = {
	{ .div = 2, .val = 3, .flags = RATE_IN_3XXX },
	{ .div = 4, .val = 4, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 5, .flags = RATE_IN_3XXX },
	{ .div = 10, .val = 6, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel_rate usim_120m_rates[] = {
	{ .div = 4, .val = 7, .flags = RATE_IN_3XXX },
	{ .div = 8, .val = 8, .flags = RATE_IN_3XXX },
	{ .div = 16, .val = 9, .flags = RATE_IN_3XXX },
	{ .div = 20, .val = 10, .flags = RATE_IN_3XXX },
	{ .div = 0 }
};

static const struct clksel usim_clksel[] = {
	{ .parent = &omap_96m_fck, .rates = usim_96m_rates },
	{ .parent = &dpll5_m2_ck, .rates = usim_120m_rates },
	{ .parent = &sys_ck, .rates = div2_rates },
	{ .parent = NULL },
};

static const char *usim_fck_parent_names[] = {
	"omap_96m_fck",
	"dpll5_m2_ck",
	"sys_ck",
};

static struct clk usim_fck;

static const struct clk_ops usim_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
	.recalc_rate	= &omap2_clksel_recalc,
	.get_parent	= &omap2_init_clksel_parent,
	.set_parent	= &omap2_clksel_set_parent,
};

static struct clk_hw_omap usim_fck_hw = {
	.hw = {
		.clk = &usim_fck,
	},
	.clksel		= usim_clksel,
	.clksel_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP3430ES2_CLKSEL_USIMOCP_MASK,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430ES2_EN_USIMOCP_SHIFT,
};

static struct clk usim_fck = {
	.name		= "usim_fck",
	.hw		= &usim_fck_hw.hw,
	.parent_names	= usim_fck_parent_names,
	.num_parents	= ARRAY_SIZE(usim_fck_parent_names),
	.ops		= &usim_fck_ops,
};

static struct clk usim_ick;

static const char *usim_ick_parent_names[] = {
	"wkup_l4_ick",
};

static const struct clk_ops usim_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap usim_ick_hw = {
	.hw = {
		.clk = &usim_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430ES2_EN_USIMOCP_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk usim_ick = {
	.name		= "usim_ick",
	.hw		= &usim_ick_hw.hw,
	.parent_names	= usim_ick_parent_names,
	.num_parents	= ARRAY_SIZE(usim_ick_parent_names),
	.ops		= &usim_ick_ops,
};

static struct clk vpfe_fck;

static const char *vpfe_fck_parent_names[] = {
	"pclk_ck",
};

static const struct clk_ops vpfe_fck_ops = {
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap vpfe_fck_hw = {
	.hw = {
		.clk = &vpfe_fck,
	},
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_VPFE_FCLK_SHIFT,
};

static struct clk vpfe_fck = {
	.name		= "vpfe_fck",
	.hw		= &vpfe_fck_hw.hw,
	.parent_names	= vpfe_fck_parent_names,
	.num_parents	= ARRAY_SIZE(vpfe_fck_parent_names),
	.ops		= &vpfe_fck_ops,
};

static struct clk vpfe_ick;

static const char *vpfe_ick_parent_names[] = {
	"ipss_ick",
};

static const struct clk_ops vpfe_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap vpfe_ick_hw = {
	.hw = {
		.clk = &vpfe_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP343X_CTRL_REGADDR(AM35XX_CONTROL_IPSS_CLK_CTRL),
	.enable_bit	= AM35XX_VPFE_VBUSP_CLK_SHIFT,
	.clkdm_name	= "core_l3_clkdm",
};

static struct clk vpfe_ick = {
	.name		= "vpfe_ick",
	.hw		= &vpfe_ick_hw.hw,
	.parent_names	= vpfe_ick_parent_names,
	.num_parents	= ARRAY_SIZE(vpfe_ick_parent_names),
	.ops		= &vpfe_ick_ops,
};

static struct clk wdt1_fck;

static const char *wdt1_fck_parent_names[] = {
	"secure_32k_fck",
};

static const struct clk_ops wdt1_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
};

static struct clk_hw_omap wdt1_fck_hw = {
	.hw = {
		.clk = &wdt1_fck,
	},
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wdt1_fck = {
	.name		= "wdt1_fck",
	.hw		= &wdt1_fck_hw.hw,
	.parent_names	= wdt1_fck_parent_names,
	.num_parents	= ARRAY_SIZE(wdt1_fck_parent_names),
	.ops		= &wdt1_fck_ops,
};

static struct clk wdt1_ick;

static const char *wdt1_ick_parent_names[] = {
	"wkup_l4_ick",
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
	.enable_bit	= OMAP3430_EN_WDT1_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wdt1_ick = {
	.name		= "wdt1_ick",
	.hw		= &wdt1_ick_hw.hw,
	.parent_names	= wdt1_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt1_ick_parent_names),
	.ops		= &wdt1_ick_ops,
};

static struct clk wdt2_fck;

static const char *wdt2_fck_parent_names[] = {
	"wkup_32k_fck",
};

static const struct clk_ops wdt2_fck_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt2_fck_hw = {
	.hw = {
		.clk = &wdt2_fck,
	},
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_WDT2_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wdt2_fck = {
	.name		= "wdt2_fck",
	.hw		= &wdt2_fck_hw.hw,
	.parent_names	= wdt2_fck_parent_names,
	.num_parents	= ARRAY_SIZE(wdt2_fck_parent_names),
	.ops		= &wdt2_fck_ops,
};

static struct clk wdt2_ick;

static const char *wdt2_ick_parent_names[] = {
	"wkup_l4_ick",
};

static const struct clk_ops wdt2_ick_ops = {
	.init		= &omap2_init_clk_clkdm,
	.enable		= &omap2_dflt_clk_enable,
	.disable	= &omap2_dflt_clk_disable,
};

static struct clk_hw_omap wdt2_ick_hw = {
	.hw = {
		.clk = &wdt2_ick,
	},
	.allow_idle	= &omap2_clkt_iclk_allow_idle,
	.deny_idle	= &omap2_clkt_iclk_deny_idle,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_WDT2_SHIFT,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk wdt2_ick = {
	.name		= "wdt2_ick",
	.hw		= &wdt2_ick_hw.hw,
	.parent_names	= wdt2_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt2_ick_parent_names),
	.ops		= &wdt2_ick_ops,
};

static struct clk wdt3_fck;

static const char *wdt3_fck_parent_names[] = {
	"per_32k_alwon_fck",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_FCLKEN),
	.enable_bit	= OMAP3430_EN_WDT3_SHIFT,
	.clkdm_name	= "per_clkdm",
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
	"per_l4_ick",
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
	.enable_reg	= OMAP_CM_REGADDR(OMAP3430_PER_MOD, CM_ICLKEN),
	.enable_bit	= OMAP3430_EN_WDT3_SHIFT,
	.clkdm_name	= "per_clkdm",
};

static struct clk wdt3_ick = {
	.name		= "wdt3_ick",
	.hw		= &wdt3_ick_hw.hw,
	.parent_names	= wdt3_ick_parent_names,
	.num_parents	= ARRAY_SIZE(wdt3_ick_parent_names),
	.ops		= &wdt3_ick_ops,
};

/*
 * clkdev
 */
static struct omap_clk omap3xxx_clks[] = {
	CLK(NULL,	"apb_pclk",	&dummy_apb_pclk,	CK_3XXX),
	CLK(NULL,	"omap_32k_fck",	&omap_32k_fck,	CK_3XXX),
	CLK(NULL,	"virt_12m_ck",	&virt_12m_ck,	CK_3XXX),
	CLK(NULL,	"virt_13m_ck",	&virt_13m_ck,	CK_3XXX),
	CLK(NULL,	"virt_16_8m_ck", &virt_16_8m_ck, CK_3430ES2PLUS | CK_AM35XX  | CK_36XX),
	CLK(NULL,	"virt_19_2m_ck", &virt_19_2m_ck, CK_3XXX),
	CLK(NULL,	"virt_26m_ck",	&virt_26m_ck,	CK_3XXX),
	CLK(NULL,	"virt_38_4m_ck", &virt_38_4m_ck, CK_3XXX),
	CLK(NULL,	"osc_sys_ck",	&osc_sys_ck,	CK_3XXX),
	CLK(NULL,	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK(NULL,	"sys_altclk",	&sys_altclk,	CK_3XXX),
	CLK("omap-mcbsp.1",	"pad_fck",	&mcbsp_clks,	CK_3XXX),
	CLK("omap-mcbsp.2",	"pad_fck",	&mcbsp_clks,	CK_3XXX),
	CLK("omap-mcbsp.3",	"pad_fck",	&mcbsp_clks,	CK_3XXX),
	CLK("omap-mcbsp.4",	"pad_fck",	&mcbsp_clks,	CK_3XXX),
	CLK("omap-mcbsp.5",	"pad_fck",	&mcbsp_clks,	CK_3XXX),
	CLK(NULL,	"mcbsp_clks",	&mcbsp_clks,	CK_3XXX),
	CLK(NULL,	"sys_clkout1",	&sys_clkout1,	CK_3XXX),
	CLK(NULL,	"dpll1_ck",	&dpll1_ck,	CK_3XXX),
	CLK(NULL,	"dpll1_x2_ck",	&dpll1_x2_ck,	CK_3XXX),
	CLK(NULL,	"dpll1_x2m2_ck", &dpll1_x2m2_ck, CK_3XXX),
	CLK(NULL,	"dpll2_ck",	&dpll2_ck,	CK_34XX | CK_36XX),
	CLK(NULL,	"dpll2_m2_ck",	&dpll2_m2_ck,	CK_34XX | CK_36XX),
	CLK(NULL,	"dpll3_ck",	&dpll3_ck,	CK_3XXX),
	CLK(NULL,	"core_ck",	&core_ck,	CK_3XXX),
	CLK(NULL,	"dpll3_x2_ck",	&dpll3_x2_ck,	CK_3XXX),
	CLK(NULL,	"dpll3_m2_ck",	&dpll3_m2_ck,	CK_3XXX),
	CLK(NULL,	"dpll3_m2x2_ck", &dpll3_m2x2_ck, CK_3XXX),
	CLK(NULL,	"dpll3_m3_ck",	&dpll3_m3_ck,	CK_3XXX),
	CLK(NULL,	"dpll3_m3x2_ck", &dpll3_m3x2_ck, CK_3XXX),
	CLK("etb",	"emu_core_alwon_ck", &emu_core_alwon_ck, CK_3XXX),
	CLK(NULL,	"dpll4_ck",	&dpll4_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_x2_ck",	&dpll4_x2_ck,	CK_3XXX),
	CLK(NULL,	"omap_192m_alwon_fck", &omap_192m_alwon_fck, CK_36XX),
	CLK(NULL,	"omap_96m_alwon_fck", &omap_96m_alwon_fck, CK_3XXX),
	CLK(NULL,	"omap_96m_fck",	&omap_96m_fck,	CK_3XXX),
	CLK(NULL,	"cm_96m_fck",	&cm_96m_fck,	CK_3XXX),
	CLK(NULL,	"omap_54m_fck",	&omap_54m_fck,	CK_3XXX),
	CLK(NULL,	"omap_48m_fck",	&omap_48m_fck,	CK_3XXX),
	CLK(NULL,	"omap_12m_fck",	&omap_12m_fck,	CK_3XXX),
	CLK(NULL,	"dpll4_m2_ck",	&dpll4_m2_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_m2x2_ck", &dpll4_m2x2_ck, CK_3XXX),
	CLK(NULL,	"dpll4_m3_ck",	&dpll4_m3_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_m3x2_ck", &dpll4_m3x2_ck, CK_3XXX),
	CLK(NULL,	"dpll4_m4_ck",	&dpll4_m4_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_m4x2_ck", &dpll4_m4x2_ck, CK_3XXX),
	CLK(NULL,	"dpll4_m5_ck",	&dpll4_m5_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_m5x2_ck", &dpll4_m5x2_ck, CK_3XXX),
	CLK(NULL,	"dpll4_m6_ck",	&dpll4_m6_ck,	CK_3XXX),
	CLK(NULL,	"dpll4_m6x2_ck", &dpll4_m6x2_ck, CK_3XXX),
	CLK("etb",	"emu_per_alwon_ck", &emu_per_alwon_ck, CK_3XXX),
	CLK(NULL,	"dpll5_ck",	&dpll5_ck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"dpll5_m2_ck",	&dpll5_m2_ck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"clkout2_src_ck", &clkout2_src_ck, CK_3XXX),
	CLK(NULL,	"sys_clkout2",	&sys_clkout2,	CK_3XXX),
	CLK(NULL,	"corex2_fck",	&corex2_fck,	CK_3XXX),
	CLK(NULL,	"dpll1_fck",	&dpll1_fck,	CK_3XXX),
	CLK(NULL,	"mpu_ck",	&mpu_ck,	CK_3XXX),
	CLK(NULL,	"arm_fck",	&arm_fck,	CK_3XXX),
	CLK("etb",	"emu_mpu_alwon_ck", &emu_mpu_alwon_ck, CK_3XXX),
	CLK(NULL,	"dpll2_fck",	&dpll2_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"iva2_ck",	&iva2_ck,	CK_34XX | CK_36XX),
	CLK(NULL,	"l3_ick",	&l3_ick,	CK_3XXX),
	CLK(NULL,	"l4_ick",	&l4_ick,	CK_3XXX),
	CLK(NULL,	"rm_ick",	&rm_ick,	CK_3XXX),
	CLK(NULL,	"gfx_l3_ck",	&gfx_l3_ck,	CK_3430ES1),
	CLK(NULL,	"gfx_l3_fck",	&gfx_l3_fck,	CK_3430ES1),
	CLK(NULL,	"gfx_l3_ick",	&gfx_l3_ick,	CK_3430ES1),
	CLK(NULL,	"gfx_cg1_ck",	&gfx_cg1_ck,	CK_3430ES1),
	CLK(NULL,	"gfx_cg2_ck",	&gfx_cg2_ck,	CK_3430ES1),
	CLK(NULL,	"sgx_fck",	&sgx_fck,	CK_3430ES2PLUS | CK_3517 | CK_36XX),
	CLK(NULL,	"sgx_ick",	&sgx_ick,	CK_3430ES2PLUS | CK_3517 | CK_36XX),
	CLK(NULL,	"d2d_26m_fck",	&d2d_26m_fck,	CK_3430ES1),
	CLK(NULL,	"modem_fck",	&modem_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"sad2d_ick",	&sad2d_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"mad2d_ick",	&mad2d_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"gpt10_fck",	&gpt10_fck,	CK_3XXX),
	CLK(NULL,	"gpt11_fck",	&gpt11_fck,	CK_3XXX),
	CLK(NULL,	"cpefuse_fck",	&cpefuse_fck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"ts_fck",	&ts_fck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"usbtll_fck",	&usbtll_fck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("usbhs_omap",	"usbtll_fck",	&usbtll_fck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("omap-mcbsp.1",	"prcm_fck",	&core_96m_fck,	CK_3XXX),
	CLK("omap-mcbsp.5",	"prcm_fck",	&core_96m_fck,	CK_3XXX),
	CLK(NULL,	"core_96m_fck",	&core_96m_fck,	CK_3XXX),
	CLK(NULL,	"mmchs3_fck",	&mmchs3_fck,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"mmchs2_fck",	&mmchs2_fck,	CK_3XXX),
	CLK(NULL,	"mspro_fck",	&mspro_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"mmchs1_fck",	&mmchs1_fck,	CK_3XXX),
	CLK(NULL,	"i2c3_fck",	&i2c3_fck,	CK_3XXX),
	CLK(NULL,	"i2c2_fck",	&i2c2_fck,	CK_3XXX),
	CLK(NULL,	"i2c1_fck",	&i2c1_fck,	CK_3XXX),
	CLK(NULL,	"mcbsp5_fck",	&mcbsp5_fck,	CK_3XXX),
	CLK(NULL,	"mcbsp1_fck",	&mcbsp1_fck,	CK_3XXX),
	CLK(NULL,	"core_48m_fck",	&core_48m_fck,	CK_3XXX),
	CLK(NULL,	"mcspi4_fck",	&mcspi4_fck,	CK_3XXX),
	CLK(NULL,	"mcspi3_fck",	&mcspi3_fck,	CK_3XXX),
	CLK(NULL,	"mcspi2_fck",	&mcspi2_fck,	CK_3XXX),
	CLK(NULL,	"mcspi1_fck",	&mcspi1_fck,	CK_3XXX),
	CLK(NULL,	"uart2_fck",	&uart2_fck,	CK_3XXX),
	CLK(NULL,	"uart1_fck",	&uart1_fck,	CK_3XXX),
	CLK(NULL,	"fshostusb_fck", &fshostusb_fck, CK_3430ES1),
	CLK(NULL,	"core_12m_fck",	&core_12m_fck,	CK_3XXX),
	CLK("omap_hdq.0",	"fck",	&hdq_fck,	CK_3XXX),
	CLK(NULL,	"ssi_ssr_fck",	&ssi_ssr_fck_3430es1,	CK_3430ES1),
	CLK(NULL,	"ssi_ssr_fck",	&ssi_ssr_fck_3430es2,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"ssi_sst_fck",	&ssi_sst_fck_3430es1,	CK_3430ES1),
	CLK(NULL,	"ssi_sst_fck",	&ssi_sst_fck_3430es2,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"core_l3_ick",	&core_l3_ick,	CK_3XXX),
	CLK("musb-omap2430",	"ick",	&hsotgusb_ick_3430es1,	CK_3430ES1),
	CLK("musb-omap2430",	"ick",	&hsotgusb_ick_3430es2,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"hsotgusb_ick",	&hsotgusb_ick_3430es1,	CK_3430ES1),
	CLK(NULL,	"hsotgusb_ick",	&hsotgusb_ick_3430es2,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"sdrc_ick",	&sdrc_ick,	CK_3XXX),
	CLK(NULL,	"gpmc_fck",	&gpmc_fck,	CK_3XXX),
	CLK(NULL,	"security_l3_ick", &security_l3_ick, CK_34XX | CK_36XX),
	CLK(NULL,	"pka_ick",	&pka_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"core_l4_ick",	&core_l4_ick,	CK_3XXX),
	CLK(NULL,	"usbtll_ick",	&usbtll_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("usbhs_omap",	"usbtll_ick",	&usbtll_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("omap_hsmmc.2",	"ick",	&mmchs3_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"mmchs3_ick",	&mmchs3_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"icr_ick",	&icr_ick,	CK_34XX | CK_36XX),
	CLK("omap-aes",	"ick",	&aes2_ick,	CK_34XX | CK_36XX),
	CLK("omap-sham",	"ick",	&sha12_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"des2_ick",	&des2_ick,	CK_34XX | CK_36XX),
	CLK("omap_hsmmc.1",	"ick",	&mmchs2_ick,	CK_3XXX),
	CLK("omap_hsmmc.0",	"ick",	&mmchs1_ick,	CK_3XXX),
	CLK(NULL,	"mmchs2_ick",	&mmchs2_ick,	CK_3XXX),
	CLK(NULL,	"mmchs1_ick",	&mmchs1_ick,	CK_3XXX),
	CLK(NULL,	"mspro_ick",	&mspro_ick,	CK_34XX | CK_36XX),
	CLK("omap_hdq.0", "ick",	&hdq_ick,	CK_3XXX),
	CLK("omap2_mcspi.4", "ick",	&mcspi4_ick,	CK_3XXX),
	CLK("omap2_mcspi.3", "ick",	&mcspi3_ick,	CK_3XXX),
	CLK("omap2_mcspi.2", "ick",	&mcspi2_ick,	CK_3XXX),
	CLK("omap2_mcspi.1", "ick",	&mcspi1_ick,	CK_3XXX),
	CLK(NULL, 	"mcspi4_ick",	&mcspi4_ick,	CK_3XXX),
	CLK(NULL, 	"mcspi3_ick",	&mcspi3_ick,	CK_3XXX),
	CLK(NULL, 	"mcspi2_ick",	&mcspi2_ick,	CK_3XXX),
	CLK(NULL, 	"mcspi1_ick",	&mcspi1_ick,	CK_3XXX),
	CLK("omap_i2c.3", "ick",	&i2c3_ick,	CK_3XXX),
	CLK("omap_i2c.2", "ick",	&i2c2_ick,	CK_3XXX),
	CLK("omap_i2c.1", "ick",	&i2c1_ick,	CK_3XXX),
	CLK(NULL, 	"i2c3_ick",	&i2c3_ick,	CK_3XXX),
	CLK(NULL, 	"i2c2_ick",	&i2c2_ick,	CK_3XXX),
	CLK(NULL, 	"i2c1_ick",	&i2c1_ick,	CK_3XXX),
	CLK(NULL,	"uart2_ick",	&uart2_ick,	CK_3XXX),
	CLK(NULL,	"uart1_ick",	&uart1_ick,	CK_3XXX),
	CLK(NULL,	"gpt11_ick",	&gpt11_ick,	CK_3XXX),
	CLK(NULL,	"gpt10_ick",	&gpt10_ick,	CK_3XXX),
	CLK("omap-mcbsp.5", "ick",	&mcbsp5_ick,	CK_3XXX),
	CLK("omap-mcbsp.1", "ick",	&mcbsp1_ick,	CK_3XXX),
	CLK(NULL, 	"mcbsp5_ick",	&mcbsp5_ick,	CK_3XXX),
	CLK(NULL, 	"mcbsp1_ick",	&mcbsp1_ick,	CK_3XXX),
	CLK(NULL,	"fac_ick",	&fac_ick,	CK_3430ES1),
	CLK(NULL,	"mailboxes_ick", &mailboxes_ick, CK_34XX | CK_36XX),
	CLK(NULL,	"omapctrl_ick",	&omapctrl_ick,	CK_3XXX),
	CLK(NULL,	"ssi_l4_ick",	&ssi_l4_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"ssi_ick",	&ssi_ick_3430es1,	CK_3430ES1),
	CLK(NULL,	"ssi_ick",	&ssi_ick_3430es2,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"usb_l4_ick",	&usb_l4_ick,	CK_3430ES1),
	CLK(NULL,	"security_l4_ick2", &security_l4_ick2, CK_34XX | CK_36XX),
	CLK(NULL,	"aes1_ick",	&aes1_ick,	CK_34XX | CK_36XX),
	CLK("omap_rng",	"ick",		&rng_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"sha11_ick",	&sha11_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"des1_ick",	&des1_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"dss1_alwon_fck",		&dss1_alwon_fck_3430es1, CK_3430ES1),
	CLK(NULL,	"dss1_alwon_fck",		&dss1_alwon_fck_3430es2, CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"dss_tv_fck",	&dss_tv_fck,	CK_3XXX),
	CLK(NULL,	"dss_96m_fck",	&dss_96m_fck,	CK_3XXX),
	CLK(NULL,	"dss2_alwon_fck",	&dss2_alwon_fck, CK_3XXX),
	CLK("omapdss_dss",	"ick",		&dss_ick_3430es1,	CK_3430ES1),
	CLK(NULL,	"dss_ick",		&dss_ick_3430es1,	CK_3430ES1),
	CLK("omapdss_dss",	"ick",		&dss_ick_3430es2,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"dss_ick",		&dss_ick_3430es2,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"cam_mclk",	&cam_mclk,	CK_34XX | CK_36XX),
	CLK(NULL,	"cam_ick",	&cam_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"csi2_96m_fck",	&csi2_96m_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"usbhost_120m_fck", &usbhost_120m_fck, CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"usbhost_48m_fck", &usbhost_48m_fck, CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK(NULL,	"usbhost_ick",	&usbhost_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("usbhs_omap",	"usbhost_ick",	&usbhost_ick,	CK_3430ES2PLUS | CK_AM35XX | CK_36XX),
	CLK("usbhs_omap",	"utmi_p1_gfclk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"utmi_p2_gfclk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"xclk60mhsp1_ck",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"xclk60mhsp2_ck",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"usb_host_hs_utmi_p1_clk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"usb_host_hs_utmi_p2_clk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"usb_tll_hs_usb_ch0_clk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"usb_tll_hs_usb_ch1_clk",	&dummy_ck,	CK_3XXX),
	CLK("usbhs_omap",	"init_60m_fclk",	&dummy_ck,	CK_3XXX),
	CLK(NULL,	"usim_fck",	&usim_fck,	CK_3430ES2PLUS | CK_36XX),
	CLK(NULL,	"gpt1_fck",	&gpt1_fck,	CK_3XXX),
	CLK(NULL,	"wkup_32k_fck",	&wkup_32k_fck,	CK_3XXX),
	CLK(NULL,	"gpio1_dbck",	&gpio1_dbck,	CK_3XXX),
	CLK(NULL,	"wdt2_fck",		&wdt2_fck,	CK_3XXX),
	CLK(NULL,	"wkup_l4_ick",	&wkup_l4_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"usim_ick",	&usim_ick,	CK_3430ES2PLUS | CK_36XX),
	CLK("omap_wdt",	"ick",		&wdt2_ick,	CK_3XXX),
	CLK(NULL,	"wdt2_ick",	&wdt2_ick,	CK_3XXX),
	CLK(NULL,	"wdt1_ick",	&wdt1_ick,	CK_3XXX),
	CLK(NULL,	"gpio1_ick",	&gpio1_ick,	CK_3XXX),
	CLK(NULL,	"omap_32ksync_ick", &omap_32ksync_ick, CK_3XXX),
	CLK(NULL,	"gpt12_ick",	&gpt12_ick,	CK_3XXX),
	CLK(NULL,	"gpt1_ick",	&gpt1_ick,	CK_3XXX),
	CLK("omap-mcbsp.2",	"prcm_fck",	&per_96m_fck,	CK_3XXX),
	CLK("omap-mcbsp.3",	"prcm_fck",	&per_96m_fck,	CK_3XXX),
	CLK("omap-mcbsp.4",	"prcm_fck",	&per_96m_fck,	CK_3XXX),
	CLK(NULL,	"per_96m_fck",	&per_96m_fck,	CK_3XXX),
	CLK(NULL,	"per_48m_fck",	&per_48m_fck,	CK_3XXX),
	CLK(NULL,	"uart3_fck",	&uart3_fck,	CK_3XXX),
	CLK(NULL,	"uart4_fck",	&uart4_fck,	CK_36XX),
	CLK(NULL,	"uart4_fck",	&uart4_fck_am35xx, CK_3505 | CK_3517),
	CLK(NULL,	"gpt2_fck",	&gpt2_fck,	CK_3XXX),
	CLK(NULL,	"gpt3_fck",	&gpt3_fck,	CK_3XXX),
	CLK(NULL,	"gpt4_fck",	&gpt4_fck,	CK_3XXX),
	CLK(NULL,	"gpt5_fck",	&gpt5_fck,	CK_3XXX),
	CLK(NULL,	"gpt6_fck",	&gpt6_fck,	CK_3XXX),
	CLK(NULL,	"gpt7_fck",	&gpt7_fck,	CK_3XXX),
	CLK(NULL,	"gpt8_fck",	&gpt8_fck,	CK_3XXX),
	CLK(NULL,	"gpt9_fck",	&gpt9_fck,	CK_3XXX),
	CLK(NULL,	"per_32k_alwon_fck", &per_32k_alwon_fck, CK_3XXX),
	CLK(NULL,	"gpio6_dbck",	&gpio6_dbck,	CK_3XXX),
	CLK(NULL,	"gpio5_dbck",	&gpio5_dbck,	CK_3XXX),
	CLK(NULL,	"gpio4_dbck",	&gpio4_dbck,	CK_3XXX),
	CLK(NULL,	"gpio3_dbck",	&gpio3_dbck,	CK_3XXX),
	CLK(NULL,	"gpio2_dbck",	&gpio2_dbck,	CK_3XXX),
	CLK(NULL,	"wdt3_fck",	&wdt3_fck,	CK_3XXX),
	CLK(NULL,	"per_l4_ick",	&per_l4_ick,	CK_3XXX),
	CLK(NULL,	"gpio6_ick",	&gpio6_ick,	CK_3XXX),
	CLK(NULL,	"gpio5_ick",	&gpio5_ick,	CK_3XXX),
	CLK(NULL,	"gpio4_ick",	&gpio4_ick,	CK_3XXX),
	CLK(NULL,	"gpio3_ick",	&gpio3_ick,	CK_3XXX),
	CLK(NULL,	"gpio2_ick",	&gpio2_ick,	CK_3XXX),
	CLK(NULL,	"wdt3_ick",	&wdt3_ick,	CK_3XXX),
	CLK(NULL,	"uart3_ick",	&uart3_ick,	CK_3XXX),
	CLK(NULL,	"uart4_ick",	&uart4_ick,	CK_36XX),
	CLK(NULL,	"gpt9_ick",	&gpt9_ick,	CK_3XXX),
	CLK(NULL,	"gpt8_ick",	&gpt8_ick,	CK_3XXX),
	CLK(NULL,	"gpt7_ick",	&gpt7_ick,	CK_3XXX),
	CLK(NULL,	"gpt6_ick",	&gpt6_ick,	CK_3XXX),
	CLK(NULL,	"gpt5_ick",	&gpt5_ick,	CK_3XXX),
	CLK(NULL,	"gpt4_ick",	&gpt4_ick,	CK_3XXX),
	CLK(NULL,	"gpt3_ick",	&gpt3_ick,	CK_3XXX),
	CLK(NULL,	"gpt2_ick",	&gpt2_ick,	CK_3XXX),
	CLK("omap-mcbsp.2", "ick",	&mcbsp2_ick,	CK_3XXX),
	CLK("omap-mcbsp.3", "ick",	&mcbsp3_ick,	CK_3XXX),
	CLK("omap-mcbsp.4", "ick",	&mcbsp4_ick,	CK_3XXX),
	CLK(NULL, 	"mcbsp4_ick",	&mcbsp2_ick,	CK_3XXX),
	CLK(NULL, 	"mcbsp3_ick",	&mcbsp3_ick,	CK_3XXX),
	CLK(NULL, 	"mcbsp2_ick",	&mcbsp4_ick,	CK_3XXX),
	CLK(NULL,	"mcbsp2_fck",	&mcbsp2_fck,	CK_3XXX),
	CLK(NULL,	"mcbsp3_fck",	&mcbsp3_fck,	CK_3XXX),
	CLK(NULL,	"mcbsp4_fck",	&mcbsp4_fck,	CK_3XXX),
	CLK("etb",	"emu_src_ck",	&emu_src_ck,	CK_3XXX),
	CLK(NULL,	"pclk_fck",	&pclk_fck,	CK_3XXX),
	CLK(NULL,	"pclkx2_fck",	&pclkx2_fck,	CK_3XXX),
	CLK(NULL,	"atclk_fck",	&atclk_fck,	CK_3XXX),
	CLK(NULL,	"traceclk_src_fck", &traceclk_src_fck, CK_3XXX),
	CLK(NULL,	"traceclk_fck",	&traceclk_fck,	CK_3XXX),
	CLK(NULL,	"sr1_fck",	&sr1_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"sr2_fck",	&sr2_fck,	CK_34XX | CK_36XX),
	CLK(NULL,	"sr_l4_ick",	&sr_l4_ick,	CK_34XX | CK_36XX),
	CLK(NULL,	"secure_32k_fck", &secure_32k_fck, CK_3XXX),
	CLK(NULL,	"gpt12_fck",	&gpt12_fck,	CK_3XXX),
	CLK(NULL,	"wdt1_fck",	&wdt1_fck,	CK_3XXX),
	CLK(NULL,	"ipss_ick",	&ipss_ick,	CK_AM35XX),
	CLK(NULL,	"rmii_ck",	&rmii_ck,	CK_AM35XX),
	CLK(NULL,	"pclk_ck",	&pclk_ck,	CK_AM35XX),
	CLK("davinci_emac",	NULL,	&emac_ick,	CK_AM35XX),
	CLK("davinci_mdio.0",	NULL,	&emac_fck,	CK_AM35XX),
	CLK("vpfe-capture",	"master",	&vpfe_ick,	CK_AM35XX),
	CLK("vpfe-capture",	"slave",	&vpfe_fck,	CK_AM35XX),
	CLK("musb-am35x",	"ick",		&hsotgusb_ick_am35xx,	CK_AM35XX),
	CLK(NULL,	"hsotgusb_ick",		&hsotgusb_ick_am35xx,	CK_AM35XX),
	CLK("musb-am35x",	"fck",		&hsotgusb_fck_am35xx,	CK_AM35XX),
	CLK(NULL,	"hecc_ck",	&hecc_ck,	CK_AM35XX),
	CLK(NULL,	"uart4_ick",	&uart4_ick_am35xx,	CK_AM35XX),
	CLK("omap_timer.1",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.2",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.3",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.4",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.5",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.6",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.7",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.8",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.9",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.10",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.11",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.12",	"32k_ck",	&omap_32k_fck,  CK_3XXX),
	CLK("omap_timer.1",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.2",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.3",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.4",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.5",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.6",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.7",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.8",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.9",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.10",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.11",	"sys_ck",	&sys_ck,	CK_3XXX),
	CLK("omap_timer.12",	"sys_ck",	&sys_ck,	CK_3XXX),
};


int __init omap3xxx_clk_init(void)
{
	struct omap_clk *c;
	u32 cpu_clkflg = 0;

	/*
	 * 3505 must be tested before 3517, since 3517 returns true
	 * for both AM3517 chips and AM3517 family chips, which
	 * includes 3505.  Unfortunately there's no obvious family
	 * test for 3517/3505 :-(
	 */
	if (cpu_is_omap3505()) {
		cpu_mask = RATE_IN_34XX;
		cpu_clkflg = CK_3505;
	} else if (cpu_is_omap3517()) {
		cpu_mask = RATE_IN_34XX;
		cpu_clkflg = CK_3517;
	} else if (cpu_is_omap3505()) {
		cpu_mask = RATE_IN_34XX;
		cpu_clkflg = CK_3505;
	} else if (cpu_is_omap3630()) {
		cpu_mask = (RATE_IN_34XX | RATE_IN_36XX);
		cpu_clkflg = CK_36XX;
	} else if (cpu_is_ti816x()) {
		cpu_mask = RATE_IN_TI816X;
		cpu_clkflg = CK_TI816X;
	} else if (cpu_is_am33xx()) {
		cpu_mask = RATE_IN_AM33XX;
	} else if (cpu_is_ti814x()) {
		cpu_mask = RATE_IN_TI814X;
	} else if (cpu_is_omap34xx()) {
		if (omap_rev() == OMAP3430_REV_ES1_0) {
			cpu_mask = RATE_IN_3430ES1;
			cpu_clkflg = CK_3430ES1;
		} else {
			/*
			 * Assume that anything that we haven't matched yet
			 * has 3430ES2-type clocks.
			 */
			cpu_mask = RATE_IN_3430ES2PLUS;
			cpu_clkflg = CK_3430ES2PLUS;
		}
	} else {
		WARN(1, "clock: could not identify OMAP3 variant\n");
	}

	if (omap3_has_192mhz_clk())
		omap_96m_alwon_fck = omap_96m_alwon_fck_3630;
	/*
	 * XXX This type of dynamic rewriting of the clock tree is
	 * deprecated and should be revised soon.
	 */
	if (cpu_is_omap3630())
		dpll4_dd = dpll4_dd_3630;
	else
		dpll4_dd = dpll4_dd_34xx;

	for (c = omap3xxx_clks; c < omap3xxx_clks + ARRAY_SIZE(omap3xxx_clks);
	     c++)
		if (c->cpu & cpu_clkflg) {
			clkdev_add(&c->lk);
			__clk_init(NULL, c->lk.clk);
		}
	return 0;
}
