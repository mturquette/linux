/*
 * Copyright (C) 2015 BayLibre, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Unit tests for the Coordinated Clock Rates
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

struct test_clk {
	struct clk_hw hw;
	unsigned long rate;
	int div;
};

#define NR_CLK  2
#define NR_RATE 3

/* clk_ops */

static inline struct test_clk *to_test_clk(struct clk_hw *hw)
{
	return container_of(hw, struct test_clk, hw);
}

static unsigned long test_clk_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct test_clk *test_clk = to_test_clk(hw);

	return test_clk->rate;
}

static int test_coordinate_rates(const struct coord_rate_domain *crd,
		int rate_idx) {
	int clk_idx;

	for (clk_idx = 0; clk_idx < crd->nr_clks; clk_idx++) {
		pr_err("%s: clk rate %lu\n", __func__,
				crd->table[clk_idx][rate_idx].rate);
	}

	return 0;
}

static const struct clk_ops test_clk_ops = {
	.recalc_rate = test_clk_recalc_rate,
	.select_coord_rates = generic_select_coord_rates,
	.coordinate_rates = test_coordinate_rates,
};

/* coordinated rates static data, shared by test_parent & test_child */

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
