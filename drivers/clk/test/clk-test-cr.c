/*
 * Copyright (C) 2015 BayLibre, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Unit tests for coordinated rates
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#if 0
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/module.h>
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
	//struct cr_state *curr_state; // optional
	/*
	 * XXX everything below here is a hack because these are fake clocks.
	 * Normally this info would come from reading the state from hardware
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
	//struct cr_domain *domain;
	struct test_clk_static *test = to_test_clk_static(hw);

	state = clk_simple_get_cr_state(hw, test->domain, rate);
	if (IS_ERR(state))
		pr_err("%s: failed to get cr_state for clk %s with code %ld\n",
				__func__, clk_hw_get_name(hw), PTR_ERR(state));

	pr_debug("%s: clk %s selected cr_state for rate %lu\n",
			__func__, clk_hw_get_name(hw), rate);
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
	 * XXX note to clock provider drivers implementers:
	 *
	 * machine-specific register writes would go here for an implementation
	 * on real hardware, perhaps making use of the cr_state->priv data
	 *
	 * everything below here is a super ugly hack because these are fake
	 * clocks. Normally this info would come from reading the hardware
	 * state. For this unit test we hard-code the pll rate, post-divider
	 * divisor, and mux parent so that .recalc and .get_parent continue to
	 * work
	 */
	test_clk = to_test_clk_static(state->clks[0]->hw);
	test_clk->pll_rate = priv->pll_rate;
	test_clk = to_test_clk_static(state->clks[1]->hw);
	test_clk->div = priv->post_divider_div;
	test_clk = to_test_clk_static(state->clks[2]->hw);
	test_clk->parent_idx = priv->cpu_mux_parent_idx;
	return 0;
}

/*
 * XXX separate ops are not always necessary, but make this test easier to read
 */
#if 0
static const struct clk_ops test_clk_static_ops = {
	.recalc_rate = test_clk_static_recalc_rate,
	.get_cr_state = test_clk_static_get_cr_state,
	.set_cr_state = test_clk_static_set_cr_state,
};
#endif

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
	//.recalc_rate = test_clk_static_recalc_rate,
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

static struct test_clk_static test_static_cpu_mux = {
	.parent_idx = 0,
	.domain = &test_static_cr_domain,
	.hw.init = &(struct clk_init_data){
		.name = "test_static_cpu_mux",
		.ops = &test_clk_mux_static_ops,
		.parent_names = (const char *[]){ "test_osc",
			"test_static_div" },
		.num_parents = 2,
	},
};

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
			.hw = &test_static_cpu_mux.hw,
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
			.hw = &test_static_cpu_mux.hw,
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
			.hw = &test_static_cpu_mux.hw,
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
 * your own data structure for tables without using struct cr_domain. The ccf
 * core does not use cr_domain at all. However, struct cr_domain does pair
 * nicely with clk_simple_get_cr_state, an optional helper function for finding
 * the first matching cr_state for a given (clk, rate) tuple.
 */
static struct cr_domain test_static_cr_domain = {
	.nr_state = 3,
	.priv = NULL,
	.states = { &state_bypass, &state_middle, &state_high, },
};

/* --- NEW SHIT --- */

/* XXX Example of how this works */
#if 0
struct my_clk_hw foo_hw {
	.hw.init = ...;
	.coord_rate_group = my_cr_group;
};

struct coord_rate_state *get_my_coord_rates(struct clk_hw *hw,
		unsigned long rate)
{
	int index = figure_out_index;
	return hw->coord_rate_group->states[index];
}

struct clk_ops my_ops = {
	.get_coord_rates = get_my_coord_rates;
};
#endif

#if 0
/*
 * FIXME
 * contrive a "safe hook" example with:
 * fixed-rate osc
 * adjustable PLL
 * cpu mux
 *
 * The only rule is that the cpu mux must reparent to the osc during pll rate
 * change/relock. Once settled we can switch the cpu back to the pll parent
 */
struct coord_rate_group coord_rate_cpu {
	.nr_state = NR_RATE;
	.states = {
		/* OPP 1 */
		/*(struct coord_rate_state [])*/{
			.nr_hws = NR_CLKS,
			.priv = NULL, // lol
			.clks = {
				{
					.hw = &test_child.hw,
					.parent_hw = &test_parent.hw,
					.rate = 11,
				},
				{
					.hw = &parent_child.hw,
					.parent_hw = &osc.hw,
					.rate = 25,
				},
				/*
				 * FIXME do we need to represent the osc in
				 * this structure? It's a transient state, a
				 * temporary re-parent, so maybe not...
				 */
#if 0
				{ .hw = &parent_child.hw,
					.parent_hw = &osc.hw,
					.rate = 100,
				},
#endif
			},
		},
		/* OPP 2 */
		/*(struct coord_rate_state [])*/{
			.nr_hws = NR_CLKS,
			.priv = NULL, // lol
			.clks = {
				{
					.hw = &test_child.hw,
					.parent_hw = &test_parent.hw,
					.rate = 33,
				},
				{ .hw = &parent_child.hw,
					.parent_hw = &osc.hw,
					.rate = 50,
				},
			},
		},
		/* OPP 3 */
		/*(struct coord_rate_state [])*/{
			.nr_hws = NR_CLKS,
			.priv = NULL, // lol
			.clks = {
				{
					.hw = &test_child.hw,
					.parent_hw = &test_parent.hw,
					.rate = 66,
				},
				{
					.hw = &parent_child.hw,
					.parent_hw = &osc.hw,
					.rate = 100,
				},
			},
		},
	};
};

/* XXX potential macro */
struct coord_rate_group coord_rate_cpu {
	.nr_state = NR_RATE;
	.states = {
		/* OPP 1 */
		CR_STATE(NR_CLKS, NULL, CR_CLKS(
				CR_CLK(&test_child.hw, &test_parent.hw, 11),
				CR_CLK(&parent_child.hw, &osc_parent.hw, 25),
				)),
		/* OPP 2 */
		CR_STATE(NR_CLKS, NULL, CR_CLKS(
				CR_CLK(&test_child.hw, &test_parent.hw, 33),
				CR_CLK(&parent_child.hw, &osc_parent.hw, 50),
				)),
		/* OPP 3 */
		CR_STATE(NR_CLKS, NULL, CR_CLKS(
				CR_CLK(&test_child.hw, &test_parent.hw, 66),
				CR_CLK(&parent_child.hw, &osc_parent.hw, 100),
				)),
	};
};
#endif
/*
 * FIXME
 * now do it again, guardian! Dynamically allocate and initialize the above
 * struct coord_rate_state!
 */

/*
 * =====================================================================
 * dynamic rate tables example
 * =====================================================================
 */



/* --- END NEW SHIT --- */

/* coordinated rates static data, shared by test_parent & test_child */

#if 0
static struct coord_rate_entry *test_tbl[] = {
	(struct coord_rate_entry []){	/* test_parent */
		{ .rate = 100, },
		{ .rate = 50,  },
		{ .rate = 25,  },
	},
	(struct coord_rate_entry []){	/* test_child */
		{ .rate = 66, .parent_rate = 100, },
		{ .rate = 33, .parent_rate = 500, },
		{ .rate = 11, .parent_rate = 25,  },
	},
};

static struct coord_rate_domain test_coord_domain = {
	.nr_clks = NR_CLK,
	.nr_rates = NR_RATE,
	.table = test_tbl,
};

/* individual clk static data */

static struct test_clk test_parent = {
	.hw.init = &(struct clk_init_data){
		.name = "test_parent",
		.parent_names = NULL,
		.num_parents = 0,
		.ops = &test_clk_ops,
		.flags = CLK_IS_ROOT,
	},
	.hw.cr_domain = &test_coord_domain,
	.hw.cr_clk_index = 0,
};

static struct test_clk test_child = {
	.hw.init = &(struct clk_init_data){
		.name = "test_child",
		.parent_names = (const char *[]){ "test_parent" },
		.num_parents = 1,
		.ops = &test_clk_ops,
	},
	.hw.cr_domain = &test_coord_domain,
	.hw.cr_clk_index = 1,
};
#endif

#if 0
static int __init clk_test_init(void)
{
	struct clk *parent, *child;
	int i, ret;

	/* FIXME convert to platform_device & devm_clk_register */

	/* assign clk_hw pointers and cr_clk_index now that we know them */
	for (i = 0; i < NR_RATE; i++) {
		test_parent.hw.cr_domain->table[test_parent.hw.cr_clk_index][i].hw = &test_parent.hw;
		test_child.hw.cr_domain->table[test_child.hw.cr_clk_index][i].hw = &test_child.hw;
		test_child.hw.cr_domain->table[test_child.hw.cr_clk_index][i].parent_hw = &test_parent.hw;
	}

	parent = clk_register(NULL, &test_parent.hw);
	child = clk_register(NULL, &test_child.hw);

	printk("---------- coordinated clk rate test results ------------\n");

	ret = clk_set_rate(child, 11);
	pr_err("ret is %d\n", ret);

	ret = clk_set_rate(child, 66);
	pr_err("ret is %d\n", ret);

	ret = clk_set_rate(child, 33);
	pr_err("ret is %d\n", ret);

	printk("---------------------------------------------------------\n");

	return 0;
}

module_init(clk_test_init);

MODULE_LICENSE("GPL");
#endif

/* -------------------------------- */

struct clk_hw *clk_test_cr_hw[] = {
	&test_osc.hw,
	&test_static_pll.hw,
	&test_static_div.hw,
	&test_static_cpu_mux.hw,
	//&test_dynamic_pll.hw,
	//&test_dynamic_div.hw,
	//&test_dynamic_cpu_mux.hw,
};

int clk_test_cr_probe(void)
//int clk_test_cr_probe(struct platform_device *pdev)
{
	int i, ret;
	struct clk *cpu_mux;
	//struct device *dev = &pdev->dev;

	pr_err("%s: I'm here!\n", __func__);

#if 0
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *child, *np = dev->of_node;
	const struct of_device_id *match;

	if (!get_scpi_ops())
		return -ENXIO;

	for_each_available_child_of_node(np, child) {
		match = of_match_node(scpi_clk_match, child);
		if (!match)
			continue;
		ret = scpi_clk_add(dev, child, match);
		if (ret) {
			clk_test_remove(pdev);
			of_node_put(child);
			return ret;
		}
	}
	/* Add the virtual cpufreq device */
	cpufreq_dev = platform_device_register_simple("scpi-cpufreq",
						      -1, NULL, 0);
	if (IS_ERR(cpufreq_dev))
		pr_warn("unable to register cpufreq device");
#endif
	/*
	 * register all clks
	 */
	for (i = 0; i < NR_CLK; i++) {
		//ret = devm_clk_hw_register(dev, clk_test_cr_hw[i]);
		ret = clk_hw_register(NULL, clk_test_cr_hw[i]);
		clk_hw_register_clkdev(clk_test_cr_hw[i], NULL, clk_test_cr_hw[i]->init->name);
		if (ret)
			pr_err("%s: unable to register test_clk_cr hw\n", __func__);
	}

	cpu_mux = clk_get(NULL, "test_static_cpu_mux");
	if (IS_ERR(cpu_mux))
		pr_err("%s: could not get cpu_mux clk\n", __func__);
	clk_set_rate(cpu_mux, 1000000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	clk_set_rate(cpu_mux, 500000000);
	pr_debug("%s: cpu_mux rate is %lu\n", __func__, clk_get_rate(cpu_mux));

	return 0;
}
