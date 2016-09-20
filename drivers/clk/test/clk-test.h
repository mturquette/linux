/*
 * Copyright (C) 2015 BayLibre, Inc <mturquette@baylibre.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * CCF unit tests
 */

#ifdef CONFIG_COMMON_CLK_TEST_CR
int clk_test_cr_probe(struct platform_device *pdev);
int clk_test_cr_remove(struct platform_device *pdev);
void clk_test_cr_shutdown(struct platform_device *pdev);
int clk_test_cr_suspend(struct platform_device *pdev, pm_message_t state);
int clk_test_cr_resume(struct platform_device *pdev);
#else
static inline int clk_test_cr_probe(struct platform_device *pdev) { return 0 }
static inline int clk_test_cr_remove(struct platform_device *pdev) { return 0}
static inline void clk_test_cr_shutdown(struct platform_device *pdev) { return 0 }
static inline int clk_test_cr_suspend(struct platform_device *pdev,
		pm_message_t state) { return 0 }
static inline int clk_test_cr_resume(struct platform_device *pdev) { return 0 }
#endif
