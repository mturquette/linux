/*
 * OMAP Power and Reset Management (PRM) driver for OMAP3xxx
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
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/omap-prm.h>

#include "omap-prm.h"

#define DRIVER_NAME "prm3xxx"

#define OMAP3_PRM_IRQSTATUS_OFFSET	0x818
#define OMAP3_PRM_IRQENABLE_OFFSET	0x81c

static const struct omap_prcm_irq_setup omap3_prcm_irq_setup = {
	.ack		= OMAP3_PRM_IRQSTATUS_OFFSET,
	.mask		= OMAP3_PRM_IRQENABLE_OFFSET,
	.nr_regs	= 1,
};

static const struct omap_prcm_irq omap3_prcm_irqs[] = {
	OMAP_PRCM_IRQ("wkup",	0,	0),
	OMAP_PRCM_IRQ("io",	9,	1),
};

static int __devinit omap3xxx_prm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct omap_prm_platform_config *pdata = pdev->dev.platform_data;

	ret = omap_prcm_register_chain_handler(pdata->irq, pdata->base,
		&omap3_prcm_irq_setup, omap3_prcm_irqs,
		ARRAY_SIZE(omap3_prcm_irqs));

	if (ret) {
		pr_err("%s: chain handler register failed: %d\n", __func__,
			ret);
	}
	return ret;
}

static int __devexit omap3xxx_prm_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver omap3xxx_prm_driver = {
	.probe		= omap3xxx_prm_probe,
	.remove		= __devexit_p(omap3xxx_prm_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRIVER_NAME,
		.pm	= &omap_prm_pm_ops,
	},
};

static int __init omap3xxx_prm_init(void)
{
	return platform_driver_register(&omap3xxx_prm_driver);
}
module_init(omap3xxx_prm_init);

static void __exit omap3xxx_prm_exit(void)
{
	platform_driver_unregister(&omap3xxx_prm_driver);
}
module_exit(omap3xxx_prm_exit);

MODULE_ALIAS("platform:"DRIVER_NAME);
MODULE_AUTHOR("Tero Kristo <t-kristo@ti.com>");
MODULE_DESCRIPTION("OMAP3xxx PRM driver");
MODULE_LICENSE("GPL");
