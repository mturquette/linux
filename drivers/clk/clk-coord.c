/*
 * Copyright (C) 2014 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Coordinated clocks helper functions
 */

#include <linux/clk-provider.h>
//#include <linux/module.h>
//#include <linux/slab.h>
//#include <linux/io.h>
//#include <linux/err.h>
//#include <linux/string.h>
//#include <linux/log2.h>

/**
 * DOC: flexible clock implementation for coordinated operations. Examples
 * include changing rates for several clock nodes that must be updated
 * simulatenously (via shadow registers or other method). Also m-to-1 gate
 * clocks where a single write operation controls multiple gates.
 *
 * Additionally the coordinated clock implementation may be used for individual
 * clock nodes where the behavior is not generic, but defined by a table of
 * acceptable combinations. Given strict operating conditions for this clock
 * node, the user need only supply the back-end operations (register write or
 * i2c message, etc).
 *
 * Traits of this clock:
 * prepare - clk_prepare may prepare this clock node, or others, or both
 * enable - clk_enable may ungate this clock node, or others, or both
 * rate - rate may be adjustable, according to a pre-defined rate table
 * parent - may have multiple parents, selected by pre-defined rate table
 */

#define to_clk_coord(_hw) container_of(_hw, struct clk_coord, hw)

//#define div_mask(d)	((1 << ((d)->width)) - 1)

int clk_coord_prepare(struct clk_hw*)
{
	return 0;
}
EXPORT_SYMBOL_GPL(clk_coord_prepare);

void clk_coord_unprepare(struct clk_hw*)
{
	return;
}
EXPORT_SYMBOL_GPL(clk_coord_unprepare);

#if 0
int clk_coord_is_prepared(struct clk_hw*)
{
}
#endif

int clk_coord_enable(struct clk_hw*)
{
	return 0;
}
EXPORT_SYMBOL_GPL(clk_coord_enable);

void clk_coord_disable(struct clk_hw*)
{
	return;
}
EXPORT_SYMBOL_GPL(clk_coord_disable);

#if 0
int clk_coord_is_enabled(struct clk_hw*)
{
}
#endif

unsigned long recalc_rate (struct clk_hw *hw,
		unsigned long parent_rate)
{
	/*
	 * determine entry in table
	 * look up this clock corresponding entry
	 * return that rate
	 */
}
EXPORT_SYMBOL_GPL(clk_coord_recalc_rate);

/**
 * clk_coord_determine_rate
long determine_rate (struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate,
		struct clk **best_parent_clk)
{
}
EXPORT_SYMBOL_GPL(clk_coord_determine_rate);

u8 get_parent (struct clk_hw *hw)
{
}
EXPORT_SYMBOL_GPL(clk_coord_get_parent);

/*
 * clk_coord_update_rates - Update all coordinated clocks to their new state
 * @hw:		clock serving as entry point to coordinated transition
 * @state:	selected state for all coordinated clocks
 */
int coord_rate(struct clk_hw *hw, struct clk_coord_state *state)
{
}

/* FIXME
int coord_rate(struct clk_hw *hw, unsigned long rate, struct clk *new_parent)
{
}
*/

/* FIXME
int coord_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, struct clk *parent)
{
}
*/

#if 0
int set_rate (struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
}
#endif

#if 0
		unsigned long rate,
		unsigned long parent_rate, u8 index)
{
}
#endif

#if 0
long round_rate (struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
}
#endif

#if 0
int set_parent (struct clk_hw *hw, u8 index)
{
}
EXPORT_SYMBOL_GPL(clk_coord_set_parent);
#endif

