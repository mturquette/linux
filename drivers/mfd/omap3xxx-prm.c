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

#define DRIVER_NAME "prm3xxx"

static int __devinit omap3xxx_prm_probe(struct platform_device *pdev)
{
	return 0;
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
