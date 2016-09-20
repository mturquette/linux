/*
 * Copyright (C) 2015 BayLibre, Inc <mturquette@baylibre.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Clock provider for CCF unit tests
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include "clk-test.h"

static int clk_test_probe(struct platform_device *pdev)
{
	clk_test_cr_probe(pdev);
	return 0;
}

static int clk_test_remove(struct platform_device *pdev)
{
	clk_test_cr_remove(pdev);
	return 0;
}

static void clk_test_shutdown(struct platform_device *pdev)
{
	clk_test_cr_shutdown(pdev);
}

static int clk_test_suspend(struct platform_device *pdev, pm_message_t state)
{
	clk_test_cr_suspend(pdev, state);
	return 0;
}

static int clk_test_resume(struct platform_device *pdev)
{
	clk_test_cr_resume(pdev);
	return 0;
}

static struct platform_driver clk_test_driver = {
	.driver	= {
		.name = "clk_test",
		//.of_match_table = clk_test_ids,
	},
	.probe = clk_test_probe,
	.remove = clk_test_remove,
	.shutdown = clk_test_shutdown,
	.suspend = clk_test_suspend,
	.resume = clk_test_resume,
};
module_platform_driver(clk_test_driver);

MODULE_AUTHOR("Michael Turquette <mturquette@baylibre.com>");
MODULE_DESCRIPTION("Common Clock Framework Unit Tests");
MODULE_LICENSE("GPL v2");
