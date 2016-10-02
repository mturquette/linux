/*
 * Copyright (C) 2015 BayLibre, Inc.
 * Michael Turquette <mturquette@baylibre.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Unit tests for coordinated rates
 */

#include <linux/clk-provider.h>
//#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/module.h>
#if 0
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/printk.h>
#endif

#define NR_CLK 4

/*
 * This unit test implements two sets of clocks and ops to test the coordinated
 * rate infrastructure:
 *
 * 1) statically initialized rate tables
 * 2) dynamic clock rates without tables
 *
 * The clock tree hierarchy used is a real example borrowed from clk-ls1x.c,
 * but the rates are contrived for this example:
 *                                 _____
 *         _______________________|     |
 * OSC ___/                       | MUX |___ CPU CLK
 *        \___ PLL ___ CPU DIV ___|     |
 *                                |_____|
 *
 * FIXME move all text below somewhere else...
 * Besides having fine-grained control over the rate at each node in this
 * graph, using coordinated rates allows the clock provider driver to precisely
 * control the order operations. For instance, a mux clock might need to
 * temporarily switch parents during a transition. The beginning and ending
 * parent are the same, but using a .set_cr_state callback give full control to
 * driver over the mux during transition.
 */

/*
 * test_osc is used in both the static tables and dynamic rates examples
 * note that test_osc is not a member of the of the cr_domain in the static
 * tables example
 */
static struct clk_fixed_rate test_osc = {
	.fixed_rate = 24000000,
	.hw.init = &(struct clk_init_data){
		.name = "test_osc",
		.num_parents = 0,
		.ops = &clk_fixed_rate_ops,
	},
};

/*
 * =====================================================================
 * static rate tables example
 * =====================================================================
 */

struct test_clk_static {
	struct clk_hw hw;
	struct cr_domain *domain;
	/*
	 * Normally the info from the three struct members below would come
	 * from reading the state from hardware. Instead we used cache values
	 * because these clocks are fake
	 */
	unsigned long pll_rate;
	int div;
	u8 parent_idx;
};

/* private data used in struct cr_state */
struct test_clk_priv_data {
	unsigned long pll_rate;
	int post_divider_div;
	u8 cpu_mux_parent_idx;
};

/* clk_ops */

/*
 * all of the clk_ops below rely on the cached values declared above in struct
 * test_clk_static. Normally these values would come from reading the hardware
 * state
 */
#define to_test_clk_static(_hw) container_of(_hw, struct test_clk_static, hw)

static unsigned long test_clk_pll_static_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct test_clk_static *test_clk_static = to_test_clk_static(hw);

	return test_clk_static->pll_rate;
}

static unsigned long test_clk_div_static_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct test_clk_static *test_clk_static = to_test_clk_static(hw);

	return parent_rate / test_clk_static->div;
}

static u8 test_clk_static_get_parent(struct clk_hw *hw)
{
	struct test_clk_static *test = to_test_clk_static(hw);

	return test->parent_idx;
}

static struct cr_state *test_clk_static_get_cr_state(struct clk_hw *hw,
		unsigned long rate)
{
	struct cr_state *state;
	struct test_clk_static *test = to_test_clk_static(hw);

	state = clk_get_cr_state_from_domain(hw, test->domain, rate);
	if (IS_ERR(state))
		pr_err("%s: failed to get cr_state for clk %s with code %ld\n",
				__func__, clk_hw_get_name(hw), PTR_ERR(state));

	return state;
}

static int test_clk_static_set_cr_state(const struct cr_state *state)
{
	int i;
	struct cr_clk *cr_clk;
	struct test_clk_static *test_clk;
	struct test_clk_priv_data *priv = state->priv;

	pr_debug("%s: setting cr_state:\n", __func__);
	for (i = 0; i < state->nr_clk; i++) {
		cr_clk = state->clks[i];
		pr_debug("%s: clk %s, rate %lu, parent %s\n", __func__,
				clk_hw_get_name(cr_clk->hw), cr_clk->rate,
				clk_hw_get_name(cr_clk->parent_hw));
	}

	/*
	 * XXX note to clock provider driver implementers:
	 *
	 * machine-specific register writes would go here for an implementation
	 * on real hardware, perhaps making use of the cr_state->priv data.
	 * After setting the hardware, the clock framework will read back this
	 * info in the usual .recalc_rate and .get_parent callbacks.
	 *
	 * For this unit test we store cached values for pll rate, post-divider
	 * divisor, and mux parent in memory so that .recalc_rate and
	 * .get_parent work correctly. Those callbacks simply return the cached
	 * values.
	 */
	test_clk = to_test_clk_static(state->clks[0]->hw);
	test_clk->pll_rate = priv->pll_rate;
	test_clk = to_test_clk_static(state->clks[1]->hw);
	test_clk->div = priv->post_divider_div;
	test_clk = to_test_clk_static(state->clks[2]->hw);
	test_clk->parent_idx = priv->cpu_mux_parent_idx;

	return 0;
}

/* separate clk_ops are not necessary here, but aid readability */

/* pll requires .recalc_rate */
static const struct clk_ops test_clk_pll_static_ops = {
	.recalc_rate = test_clk_pll_static_recalc_rate,
	.get_cr_state = test_clk_static_get_cr_state,
	.set_cr_state = test_clk_static_set_cr_state,
};

/* post divider requires .recalc_rate */
static const struct clk_ops test_clk_div_static_ops = {
	.recalc_rate = test_clk_div_static_recalc_rate,
	.get_cr_state = test_clk_static_get_cr_state,
	.set_cr_state = test_clk_static_set_cr_state,
};

/* cpu mux requires .get_parent */
static const struct clk_ops test_clk_mux_static_ops = {
	.get_parent = test_clk_static_get_parent,
	.get_cr_state = test_clk_static_get_cr_state,
	.set_cr_state = test_clk_static_set_cr_state,
};

/* forward declaration to keep things tidy */
static struct cr_domain test_static_cr_domain;

static struct test_clk_static test_static_pll = {
	.pll_rate = 1000000000,
	.domain = &test_static_cr_domain,
	.hw.init = &(struct clk_init_data){
		.name = "test_static_pll",
		.ops = &test_clk_pll_static_ops,
		.parent_names = (const char *[]){ "test_osc" },
		.num_parents = 1,
	},
};

static struct test_clk_static test_static_div = {
	.div = 1,
	.domain = &test_static_cr_domain,
	.hw.init = &(struct clk_init_data){
		.name = "test_static_div",
		.ops = &test_clk_div_static_ops,
		.parent_names = (const char *[]){ "test_static_pll" },
		.num_parents = 1,
	},
};

static struct test_clk_static test_static_mux = {
	.parent_idx = 0,
	.domain = &test_static_cr_domain,
	.hw.init = &(struct clk_init_data){
		.name = "test_static_mux",
		.ops = &test_clk_mux_static_ops,
		.parent_names = (const char *[]){ "test_osc",
			"test_static_div" },
		.num_parents = 2,
	},
};

/* machine-specific private data used in struct cr_state */
struct test_clk_priv_data bypass = { 1000000000, 2, 0 };
struct test_clk_priv_data middle = { 1000000000, 2, 1 };
struct test_clk_priv_data high = { 1000000000, 1, 1 };

/* low frequency, bypassing the pll */
static struct cr_state state_bypass = {
	.nr_clk = 3,
	.priv = &bypass,
	.needs_free = false,
	.clks = {
		&(struct cr_clk){
			.hw = &test_static_pll.hw,
			.parent_hw = &test_osc.hw,
			.rate = 1000000000,
			.is_root = true,
		},
		&(struct cr_clk){
			.hw = &test_static_div.hw,
			.parent_hw = &test_static_pll.hw,
			.rate = 500000000,
			.is_root = false,
		},
		&(struct cr_clk){
			.hw = &test_static_mux.hw,
			.parent_hw = &test_osc.hw,
			.rate = 24000000,
			.is_root = true,
		},
	},
};

/* middle frequency, dividing pll by 2 */
static struct cr_state state_middle = {
	.nr_clk = 3,
	.priv = &middle,
	.needs_free = false,
	.clks = {
		&(struct cr_clk){
			.hw = &test_static_pll.hw,
			.parent_hw = &test_osc.hw,
			.rate = 1000000000,
			.is_root = true,
		},
		&(struct cr_clk){
			.hw = &test_static_div.hw,
			.parent_hw = &test_static_pll.hw,
			.rate = 500000000,
			.is_root = false,
		},
		&(struct cr_clk){
			.hw = &test_static_mux.hw,
			.parent_hw = &test_static_div.hw,
			.rate = 500000000,
			.is_root = false,
		},
	},
};

/* high frequency at full pll rate */
static struct cr_state state_high = {
	.nr_clk = 3,
	.priv = &high,
	.needs_free = false,
	.clks = {
		&(struct cr_clk){
			.hw = &test_static_pll.hw,
			.parent_hw = &test_osc.hw,
			.rate = 1000000000,
			.is_root = true,
		},
		&(struct cr_clk){
			.hw = &test_static_div.hw,
			.parent_hw = &test_static_pll.hw,
			.rate = 1000000000,
			.is_root = false,
		},
		&(struct cr_clk){
			.hw = &test_static_mux.hw,
			.parent_hw = &test_static_div.hw,
			.rate = 1000000000,
			.is_root = false,
		},
	},
};

/*
 * XXX note to clock provider drivers implementers:
 *
 * cr_domain is an optional helper data structure. It provides a useful
 * starting point for tables of discretized rates. It is possible to invent
 * your own data structure for tables without using struct cr_domain. The clk
 * core does not use cr_domain at all. However, struct cr_domain does pair
 * nicely with clk_get_cr_state_from_domain, an optional helper function for
 * finding the first matching cr_state for a given (clk,rate) pair.
 */
static struct cr_domain test_static_cr_domain = {
	.nr_state = 3,
	.priv = NULL,
	.states = { &state_bypass, &state_middle, &state_high, },
};

/*
 * =====================================================================
 * dynamic rate tables example
 * =====================================================================
 */

/*
 * =====================================================================
 * module boilerplate
 * =====================================================================
 */

struct clk_hw *clk_test_cr_hw[] = {
	&test_osc.hw,
	&test_static_pll.hw,
	&test_static_div.hw,
	&test_static_mux.hw,
	//&test_dynamic_pll.hw,
	//&test_dynamic_div.hw,
	//&test_dynamic_cpu_mux.hw,
};

int clk_test_cr_probe(void)
{
	int i, ret;
	struct clk *cpu_mux;

	pr_err("%s: I'm here!\n", __func__);

	/*
	 * register all clks
	 */
	for (i = 0; i < NR_CLK; i++) {
		ret = clk_hw_register(NULL, clk_test_cr_hw[i]);
		clk_hw_register_clkdev(clk_test_cr_hw[i], clk_test_cr_hw[i]->init->name, NULL);
		pr_debug("%s: clk_test_cr_hw[i]->init->name %s\n", __func__, clk_test_cr_hw[i]->init->name);
		if (ret)
			pr_err("%s: unable to register test_clk_cr hw\n", __func__);
	}

	/* run the tests for static table clocks */
	cpu_mux = clk_get(NULL, "test_static_mux");
	if (IS_ERR(cpu_mux)) {
		pr_err("%s: could not get cpu_mux clk %ld\n", __func__, PTR_ERR(cpu_mux));
		return 0;
	}
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	clk_set_rate(cpu_mux, 1000000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	clk_set_rate(cpu_mux, 500000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	clk_set_rate(cpu_mux, 24000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	clk_set_rate(cpu_mux, 1000000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Michael Turquette <mturquette@baylibre.com>");
