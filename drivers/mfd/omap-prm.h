/*
 * OMAP Power and Reset Management (PRM) driver common definitions
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

#ifndef __DRIVERS_MFD_OMAP_PRM_H__
#define __DRIVERS_MFD_OMAP_PRM_H__

struct omap_prcm_irq_setup {
	u32 ack;
	u32 mask;
	int nr_regs;
};

struct omap_prcm_irq {
	const char *name;
	unsigned int offset;
	bool priority;
};

#define OMAP_PRCM_IRQ(_name, _offset, _priority) {	\
	.name = _name,					\
	.offset = _offset,				\
	.priority = _priority				\
	}

void omap_prcm_irq_cleanup(void);
int omap_prcm_register_chain_handler(int irq, void __iomem *base,
	const struct omap_prcm_irq_setup *irq_setup,
	const struct omap_prcm_irq *irqs, int nr_irqs);

#endif
