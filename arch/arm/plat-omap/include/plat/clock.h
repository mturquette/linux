/*
 * OMAP clock: data structure definitions, function prototypes, shared macros
 *
 * Copyright (C) 2004-2005, 2008-2010 Nokia Corporation
 * Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 * Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_OMAP_CLOCK_H
#define __ARCH_ARM_OMAP_CLOCK_H

#include <linux/list.h>

struct module;
struct clk;
struct clockdomain;

/**
 * struct clkops - some clock function pointers
 * @enable: fn ptr that enables the current clock in hardware
 * @disable: fn ptr that enables the current clock in hardware
 * @find_idlest: function returning the IDLEST register for the clock's IP blk
 * @find_companion: function returning the "companion" clk reg for the clock
 * @allow_idle: fn ptr that enables autoidle for the current clock in hardware
 * @deny_idle: fn ptr that disables autoidle for the current clock in hardware
 *
 * A "companion" clk is an accompanying clock to the one being queried
 * that must be enabled for the IP module connected to the clock to
 * become accessible by the hardware.  Neither @find_idlest nor
 * @find_companion should be needed; that information is IP
 * block-specific; the hwmod code has been created to handle this, but
 * until hwmod data is ready and drivers have been converted to use PM
 * runtime calls in place of clk_enable()/clk_disable(), @find_idlest and
 * @find_companion must, unfortunately, remain.
 */
struct clkops {
	int			(*enable)(struct clk *);
	void			(*disable)(struct clk *);
	void			(*find_idlest)(struct clk *, void __iomem **,
					       u8 *, u8 *);
	void			(*find_companion)(struct clk *, void __iomem **,
						  u8 *);
	void			(*allow_idle)(struct clk *);
	void			(*deny_idle)(struct clk *);
};

/*
 * struct clk.flags possibilities
 *
 * XXX document the rest of the clock flags here
 *
 * CLOCK_CLKOUTX2: (OMAP4 only) DPLL CLKOUT and CLKOUTX2 GATE_CTRL
 *     bits share the same register.  This flag allows the
 *     omap4_dpllmx*() code to determine which GATE_CTRL bit field
 *     should be used.  This is a temporary solution - a better approach
 *     would be to associate clock type-specific data with the clock,
 *     similar to the struct dpll_data approach.
 */
#define ENABLE_REG_32BIT	(1 << 0)	/* Use 32-bit access */
#define CLOCK_IDLE_CONTROL	(1 << 1)
#define CLOCK_NO_IDLE_PARENT	(1 << 2)
#define ENABLE_ON_INIT		(1 << 3)	/* Enable upon framework init */
#define INVERT_ENABLE		(1 << 4)	/* 0 enables, 1 disables */
#define CLOCK_CLKOUTX2		(1 << 5)

/**
 * struct clk - OMAP struct clk
 * @node: list_head connecting this clock into the full clock list
 * @ops: struct clkops * for this clock
 * @name: the name of the clock in the hardware (used in hwmod data and debug)
 * @parent: pointer to this clock's parent struct clk
 * @children: list_head connecting to the child clks' @sibling list_heads
 * @sibling: list_head connecting this clk to its parent clk's @children
 * @rate: current clock rate
 * @enable_reg: register to write to enable the clock (see @enable_bit)
 * @recalc: fn ptr that returns the clock's current rate
 * @set_rate: fn ptr that can change the clock's current rate
 * @round_rate: fn ptr that can round the clock's current rate
 * @init: fn ptr to do clock-specific initialization
 * @enable_bit: bitshift to write to enable/disable the clock (see @enable_reg)
 * @usecount: number of users that have requested this clock to be enabled
 * @fixed_div: when > 0, this clock's rate is its parent's rate / @fixed_div
 * @flags: see "struct clk.flags possibilities" above
 * @clksel_reg: for clksel clks, register va containing src/divisor select
 * @clksel_mask: bitmask in @clksel_reg for the src/divisor selector
 * @clksel: for clksel clks, pointer to struct clksel for this clock
 * @dpll_data: for DPLLs, pointer to struct dpll_data for this clock
 * @clkdm_name: clockdomain name that this clock is contained in
 * @clkdm: pointer to struct clockdomain, resolved from @clkdm_name at runtime
 * @rate_offset: bitshift for rate selection bitfield (OMAP1 only)
 * @src_offset: bitshift for source selection bitfield (OMAP1 only)
 *
 * XXX @rate_offset, @src_offset should probably be removed and OMAP1
 * clock code converted to use clksel.
 *
 * XXX @usecount is poorly named.  It should be "enable_count" or
 * something similar.  "users" in the description refers to kernel
 * code (core code or drivers) that have called clk_enable() and not
 * yet called clk_disable(); the usecount of parent clocks is also
 * incremented by the clock code when clk_enable() is called on child
 * clocks and decremented by the clock code when clk_disable() is
 * called on child clocks.
 *
 * XXX @clkdm, @usecount, @children, @sibling should be marked for
 * internal use only.
 *
 * @children and @sibling are used to optimize parent-to-child clock
 * tree traversals.  (child-to-parent traversals use @parent.)
 *
 * XXX The notion of the clock's current rate probably needs to be
 * separated from the clock's target rate.
 */
struct clk {
	struct list_head	node;
	const struct clkops	*ops;
	const char		*name;
	struct clk		*parent;
	struct list_head	children;
	struct list_head	sibling;	/* node for children */
	unsigned long		rate;
	void __iomem		*enable_reg;
	unsigned long		(*recalc)(struct clk *);
	int			(*set_rate)(struct clk *, unsigned long);
	long			(*round_rate)(struct clk *, unsigned long);
	void			(*init)(struct clk *);
	u8			enable_bit;
	s8			usecount;
	u8			fixed_div;
	u8			flags;
	u8			rate_offset;
	u8			src_offset;
#if defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS)
	struct dentry		*dent;	/* For visible tree hierarchy */
#endif
};

struct clk_functions {
	int		(*clk_enable)(struct clk *clk);
	void		(*clk_disable)(struct clk *clk);
	long		(*clk_round_rate)(struct clk *clk, unsigned long rate);
	int		(*clk_set_rate)(struct clk *clk, unsigned long rate);
	int		(*clk_set_parent)(struct clk *clk, struct clk *parent);
	void		(*clk_allow_idle)(struct clk *clk);
	void		(*clk_deny_idle)(struct clk *clk);
	void		(*clk_disable_unused)(struct clk *clk);
};

extern int mpurate;

extern int clk_init(struct clk_functions *custom_clocks);
extern void clk_preinit(struct clk *clk);
extern int clk_register(struct clk *clk);
extern void clk_reparent(struct clk *child, struct clk *parent);
extern void clk_unregister(struct clk *clk);
extern void propagate_rate(struct clk *clk);
extern void recalculate_root_clocks(void);
extern unsigned long followparent_recalc(struct clk *clk);
extern void clk_enable_init_clocks(void);
unsigned long omap_fixed_divisor_recalc(struct clk *clk);
extern struct clk *omap_clk_get_by_name(const char *name);
extern int omap_clk_enable_autoidle_all(void);
extern int omap_clk_disable_autoidle_all(void);

extern const struct clkops clkops_null;

extern struct clk dummy_ck;

#endif
