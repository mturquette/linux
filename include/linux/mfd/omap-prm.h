/*
 * OMAP Power and Reset Management (PRM) driver
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

#ifndef __LINUX_MFD_OMAP_PRM_H__
#define __LINUX_MFD_OMAP_PRM_H__

#ifdef CONFIG_OMAP_PRM
int omap_prcm_event_to_irq(const char *name);
#else
static inline int omap_prcm_event_to_irq(const char *name) { return -ENOENT; }
#endif

struct omap_prm_platform_config {
	int irq;
	void __iomem *base;
};

#endif
