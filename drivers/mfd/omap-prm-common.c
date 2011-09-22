/*
 * OMAP Power and Reset Management (PRM) driver common functionality
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Author: Tero Kristo <t-kristo@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include "omap-prm.h"

#define OMAP_PRCM_MAX_NR_PENDING_REG 2

struct omap_prm_device {
	const struct omap_prcm_irq_setup *irq_setup;
	const struct omap_prcm_irq *irqs;
	struct irq_chip_generic **irq_chips;
	int nr_irqs;
	u32 *saved_mask;
	u32 *priority_mask;
	int base_irq;
	int irq;
	void __iomem *base;
	int suspended;
};

static struct omap_prm_device prm_dev;

static inline u32 prm_read_reg(int offset)
{
	return __raw_readl(prm_dev.base + offset);
}

static inline void prm_write_reg(u32 value, int offset)
{
	__raw_writel(value, prm_dev.base + offset);
}

static void prm_pending_events(unsigned long *events)
{
	u32 mask, st;
	int i;

	memset(events, 0, prm_dev.irq_setup->nr_regs * sizeof(unsigned long));

	for (i = 0; i < prm_dev.irq_setup->nr_regs; i++) {
		mask = prm_read_reg(prm_dev.irq_setup->mask + i * 4);
		st = prm_read_reg(prm_dev.irq_setup->ack + i * 4);
		events[i] = mask & st;
	}
}

/*
 * Move priority events from events to priority_events array
 */
static void prm_events_filter_priority(unsigned long *events,
	unsigned long *priority_events)
{
	int i;

	for (i = 0; i < prm_dev.irq_setup->nr_regs; i++) {
		priority_events[i] = events[i] & prm_dev.priority_mask[i];
		events[i] ^= priority_events[i];
	}
}

/*
 * PRCM Interrupt Handler
 *
 * This is a common handler for the OMAP PRCM interrupts. Pending
 * interrupts are detected by a call to prm_pending_events and
 * dispatched accordingly. Clearing of the wakeup events should be
 * done by the SoC specific individual handlers.
 */
static void prcm_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned long pending[OMAP_PRCM_MAX_NR_PENDING_REG];
	unsigned long priority_pending[OMAP_PRCM_MAX_NR_PENDING_REG];
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int virtirq;
	int nr_irqs = prm_dev.irq_setup->nr_regs * 32;
	int i;

	if (prm_dev.suspended)
		for (i = 0; i < prm_dev.irq_setup->nr_regs; i++) {
			prm_dev.saved_mask[i] =
				prm_read_reg(prm_dev.irq_setup->mask + i * 4);
			prm_write_reg(0, prm_dev.irq_setup->mask + i * 4);
		}

	/*
	 * Loop until all pending irqs are handled, since
	 * generic_handle_irq() can cause new irqs to come
	 */
	while (!prm_dev.suspended) {
		prm_pending_events(pending);

		/* No bit set, then all IRQs are handled */
		if (find_first_bit(pending, nr_irqs) >= nr_irqs)
			break;

		prm_events_filter_priority(pending, priority_pending);

		/*
		 * Loop on all currently pending irqs so that new irqs
		 * cannot starve previously pending irqs
		 */

		/* Serve priority events first */
		for_each_set_bit(virtirq, priority_pending, nr_irqs)
			generic_handle_irq(prm_dev.base_irq + virtirq);

		/* Serve normal events next */
		for_each_set_bit(virtirq, pending, nr_irqs)
			generic_handle_irq(prm_dev.base_irq + virtirq);
	}
	if (chip->irq_ack)
		chip->irq_ack(&desc->irq_data);
	if (chip->irq_eoi)
		chip->irq_eoi(&desc->irq_data);
	chip->irq_unmask(&desc->irq_data);
}

/*
 * Given a PRCM event name, returns the corresponding IRQ on which the
 * handler should be registered.
 */
int omap_prcm_event_to_irq(const char *name)
{
	int i;

	for (i = 0; i < prm_dev.nr_irqs; i++)
		if (!strcmp(prm_dev.irqs[i].name, name))
			return prm_dev.base_irq + prm_dev.irqs[i].offset;

	return -ENOENT;
}

/*
 * Reverses memory allocated and other steps done by
 * omap_prcm_register_chain_handler
 */
void omap_prcm_irq_cleanup(void)
{
	int i;

	if (prm_dev.irq_chips) {
		for (i = 0; i < prm_dev.irq_setup->nr_regs; i++) {
			if (prm_dev.irq_chips[i])
				irq_remove_generic_chip(prm_dev.irq_chips[i],
					0xffffffff, 0, 0);
			prm_dev.irq_chips[i] = NULL;
		}
		kfree(prm_dev.irq_chips);
		prm_dev.irq_chips = NULL;
	}

	kfree(prm_dev.saved_mask);
	prm_dev.saved_mask = NULL;

	kfree(prm_dev.priority_mask);
	prm_dev.priority_mask = NULL;

	irq_set_chained_handler(prm_dev.irq, NULL);

	if (prm_dev.base_irq > 0)
		irq_free_descs(prm_dev.base_irq,
			prm_dev.irq_setup->nr_regs * 32);
	prm_dev.base_irq = 0;
}

/*
 * Initializes the prcm chain handler based on provided parameters.
 */
int omap_prcm_register_chain_handler(int irq, void __iomem *base,
	const struct omap_prcm_irq_setup *irq_setup,
	const struct omap_prcm_irq *irqs, int nr_irqs)
{
	int nr_regs = irq_setup->nr_regs;
	u32 mask[OMAP_PRCM_MAX_NR_PENDING_REG];
	int offset;
	int max_irq = 0;
	int i;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	if (nr_regs > OMAP_PRCM_MAX_NR_PENDING_REG) {
		pr_err("PRCM: nr_regs too large\n");
		goto err;
	}

	prm_dev.irq_setup = irq_setup;
	prm_dev.irqs = irqs;
	prm_dev.nr_irqs = nr_irqs;
	prm_dev.irq = irq;
	prm_dev.base = base;

	prm_dev.irq_chips = kzalloc(sizeof(void *) * nr_regs, GFP_KERNEL);
	prm_dev.saved_mask = kzalloc(sizeof(u32) * nr_regs, GFP_KERNEL);
	prm_dev.priority_mask = kzalloc(sizeof(u32) * nr_regs, GFP_KERNEL);

	if (!prm_dev.irq_chips || !prm_dev.saved_mask ||
	    !prm_dev.priority_mask) {
		pr_err("PRCM: kzalloc failed\n");
		goto err;
	}

	memset(mask, 0, sizeof(mask));

	for (i = 0; i < nr_irqs; i++) {
		offset = irqs[i].offset;
		mask[offset >> 5] |= 1 << (offset & 0x1f);
		if (offset > max_irq)
			max_irq = offset;
		if (irqs[i].priority)
			prm_dev.priority_mask[offset >> 5] |=
				1 << (offset & 0x1f);
	}

	irq_set_chained_handler(prm_dev.irq, prcm_irq_handler);

	prm_dev.base_irq = irq_alloc_descs(-1, 0, irq_setup->nr_regs * 32, 0);

	if (prm_dev.base_irq < 0) {
		pr_err("PRCM: failed to allocate irq descs\n");
		goto err;
	}

	for (i = 0; i <= irq_setup->nr_regs; i++) {
		gc = irq_alloc_generic_chip("PRCM", 1,
			prm_dev.base_irq + i * 32, base, handle_level_irq);

		if (!gc) {
			pr_err("PRCM: failed to allocate generic chip\n");
			goto err;
		}
		ct = gc->chip_types;
		ct->chip.irq_ack = irq_gc_ack_set_bit;
		ct->chip.irq_mask = irq_gc_mask_clr_bit;
		ct->chip.irq_unmask = irq_gc_mask_set_bit;

		ct->regs.ack = irq_setup->ack + i * 4;
		ct->regs.mask = irq_setup->mask + i * 4;

		irq_setup_generic_chip(gc, mask[i], 0, IRQ_NOREQUEST, 0);
		prm_dev.irq_chips[i] = gc;
	}

	return 0;

err:
	omap_prcm_irq_cleanup();
	return -ENOMEM;
}

static int omap_prm_prepare(struct device *kdev)
{
	prm_dev.suspended = 1;
	return 0;
}

static void omap_prm_complete(struct device *kdev)
{
	int i;

	prm_dev.suspended = 0;

	for (i = 0; i < prm_dev.irq_setup->nr_regs; i++)
		prm_write_reg(prm_dev.saved_mask[i],
			prm_dev.irq_setup->mask + i * 4);
}

const struct dev_pm_ops omap_prm_pm_ops = {
	.prepare = omap_prm_prepare,
	.complete = omap_prm_complete,
};

MODULE_AUTHOR("Tero Kristo <t-kristo@ti.com>");
MODULE_DESCRIPTION("OMAP PRM core driver");
MODULE_LICENSE("GPL");
