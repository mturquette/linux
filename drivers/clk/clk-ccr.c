/*
 * Copyright (C) 2014-2015 Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Coordinated clock rates for the common clock framework. See
 * Documentation/clk.txt
 */

#include <linux/clk-private.h>
/*
#include <linux/clk/clk-conf.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sched.h>

#include "clk.h"
*/

/* data structures */

/**
 * ccr_state - unique clock state
 * @clk: owner of this state
 * @parent: parent of clk, when in this state
 * @rate: rate of clk, when in this state
 * @top: top-most parent of clk in this same cg, when in this state
 * @cg: clk group containing clk
 */
struct ccr_state {
	struct clk *clk;
	struct clk *parent;
	unsigned long rate;
	struct clk *top;
	struct ccr_group *cg;
}

/**
 * ccr_group - unique group of ccr_states
 */
struct ccr_group {
	struct ccr_state cs_set[][];
	int cs_clk_num;
};

/* clk ops, etc */

struct clk *ccr_calc_new_rates(clk, rate)
{
	// 1 find the right state
	// 2 walk up the parent chain of clk until we hit a sub-root
	// 3 do i need to set up the clocks or make any changes here?
	// 4 return the "top" clock, which corresponds to the sub-root of clk's parent chain

	cs = clk->ops->ccr_find_state(clk, rate, NULL);
	if (!cs)
		return -EINVAL;


/* CEIL function by default */
struct ccr_state *ccr_find_state_ceil (struct clk *clk, struct clk *parent, unsigned long rate)
{
	struct ccr_state *cs;
	int i, j, match = 0;

	/* each struct clk will point to its ccr group. Walk through
	 * that group and find the matching clk+parent+rate
	 */

	/*
	 * walk through all known ccr states, looking for matching tuple
	 * of clk, parent and rate
	 */
	for (i = 0; i < clk.ccr_num_stats; i++)
		for (j=0, j < clk->ccr_num_clks; j++) {
			cs = clk->ccr_states[i][j];
			if (clk == cs->clk && parent == cs->parent &&
					rate == cs->rate)
				return cs;
		}
	return NULL;
/*
	for (i = 0; i < clk.ccr_num_stats; i++) {
		//for (j=0, j < clk->ccr_states[i]; j++) {
		for (j=0, j < clk->ccr_num_clks; j++) {
			cs = clk->ccr_states[i][j];
			if (clk == cs->clk && parent == cs->parent && rate == cs->rate)
				match = 1;
				break;
		}
		if (match)
			break;
	}
*/
};

struct ccr_state *ccr_find_state_default = ccr_find_state_ceil;


/* data, registratioon, etc --- */

/* mock-ups for data from clk provider */
struct ccr_state opp1 {
	clka {
		data;
	},
	clkb {
		data;
	},
};
/* PROBLEM! How do we know length of above array? */

/* alternative */
struct ccr_state opp1 {
	{ clka, parenta, 300000, list_head }, //  NOTE: list_head can be auto-populated
	{ clkb, clka, 300000, list_head },
};

/* What about using DT binding numbers for clocks? E.g:
 *
 * MMC_CLK_EN0
 *
 * Which comes from a header in the DT include chroot
 *
 * A special DT-centric function could do this. Or something else using
 * clk_hw or clk_init_data or something.
 */

struct ccr_node {
	struct clk *clk;
	struct clk *parent;
	unsigned long rate;
	struct list_head node;
};

int ccr_register(...)
{
	return 0;
}
