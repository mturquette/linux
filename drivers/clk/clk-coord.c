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

#if 0
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
 * clk_coord_determine_rate - select best rate and parent from table
 * @hw:			
 * @rate:		
 * @best_parent_rate:	
 * @best_parent_clk:	
 *
 * The round rate implementation selects a rate from the coordinated rates
 * table that is less than or equal to the requested rate. In the case that the
 * exact rate requested is not found in the coordinated rates table then
 * clk_coord_determine_rate finds the highest frequency entry in the table that
 * is less than the requested rate.
 */
long determine_rate (struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate,
		struct clk **best_parent_clk)
{
	/*
	 *
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

/**
 * FIXME
 * clk_coord_register - register a set of coordinated clocks
 * OR
 * clk_coord_register - register a single coordinated clock, possibly part of a set of coordinated clocks
 * OR
 * clk_coord_register - register coordinated clocks by table
 */

clk_register_coord(struct device *dev, struct clk_coord_desc *desc)
{
	struct clk_coord *coord;
	struct clk *clk;
	//struct clk_init_data init;

	/* allocate the coord */
	coord = kzalloc(sizeof(struct clk_coord), GFP_KERNEL);
	if (!coord) {
		pr_err("%s: could not allocate coordd clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	/*
	init.name = name;
	init.ops = &clk_coord_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);
	*/

	/* struct clk_coord assignments */
	coord->reg = desc->reg;
	coord->bit_idx = desc->bit_idx;
	coord->flags = desc->clk_coord_flags;
	coord->lock = desc->lock;
	/* stupid indirection because the clk registration stuff sucks */
	coord->hw.init = &desc->init;

	clk = clk_register(dev, &coord->hw);

	if (IS_ERR(clk))
		kfree(coord);

	return clk;
}
EXPORT_SYMBOL_GPL(clk_register_coord);

struct clk_coord_desc {
	struct clk_init_data init;
};

/**
 * struct clk_coord - coordinated clock node
 * @hw:		handle between struct clk and struct clk_coord
 * @hw_ops:	hardware-dependent callbacks for programming the clock hardware
 */
struct clk_coord {
	struct clk_hw *hw;
	struct clk_ops *hw_ops; // OK maybe don't do this. Use custom ops structure.
	//int		(*prepare)(struct clk_hw *hw);
	
	//void __iomem	*reg;
	//u8		bit_idx;
	//u8		flags;
	//spinlock_t	*lock;
};

const struct clk_ops clk_coord_ops = {
	.prepare = clk_coord_prepare,
	.unprepare = clk_coord_unprepare,
	.is_prepared = clk_coord_is_prepared,
	.enable = clk_coord_enable,
	.disable = clk_coord_disable,
	.is_enabled = clk_coord_is_enabled,
	.determine_rate = clk_coord_determine_rate,
	.recalc_rate = clk_coord_recalc_rate,
	.set_parent = clk_coord_set_parent,
	.get_parent = clk_coord_get_parent,
	.set_rate_and_parent = clk_coord_set_rate_and_parent,
	.get_rate = clk_coord_get_rate,
};

/**
 * FIXME
 * clk
 */






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

