/**
 * sata.c - The ahci sata device init functions
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <plat/omap_device.h>
#include <linux/dma-mapping.h>
#include <linux/ahci_platform.h>
#include <linux/clk.h>
#include <plat/sata.h>

#if defined(CONFIG_SATA_AHCI_PLATFORM) || \
	defined(CONFIG_SATA_AHCI_PLATFORM_MODULE)

#define OMAP_SATA_HWMODNAME	"sata"
#define AHCI_PLAT_DEVNAME	"ahci"

#define OMAP_SATA_PLL_CONTROL			0x00
#define OMAP_SATA_PLL_STATUS			0x04
#define OMAP_SATA_PLL_GO			0x08
#define OMAP_SATA_PLL_CONFIGURATION1		0x0c
#define OMAP_SATA_PLL_CONFIGURATION2		0x10
#define OMAP_SATA_PLL_CONFIGURATION3		0x14
#define OMAP_SATA_PLL_SSC_CONFIGURATION1	0x18
#define OMAP_SATA_PLL_SSC_CONFIGURATION2	0x1c
#define OMAP_SATA_PLL_CONFIGURATION4		0x20

#define OMAP_SATA_PLL_REF_CLK_ENABLE		(1 << 13)

/* Enable the set the pll clk of 1.5 Ghz*/
#define OMAP_SATA_PLL_CONFIGURATION1_1_5G	0x4e21e

#ifdef OMAP_SATA_PHY_PWR
/* - FIXME -
 * The sata phy power enable belongs to control module
 * for now it will be part of this driver; it should
 * seperated from the sata configuration.
 */
#define OMAP_CTRL_MODULE_CORE			0x4a002000
#define OMAP_CTRL_MODULE_CORE_SIZE		2048
#define OMAP_CTRL_SATA_PHY_POWER		0x374
#define OMAP_CTRL_SATA_EXT_MODE			0x3ac

/* Enable the 38.4 Mhz frequency */
#define SATA_PWRCTL_CLK_FREQ			(0x26 << 22)

/* Enable Tx and Rx phys */
#define SATA_PWRCTL_CLK_CMD			(3 << 14)

#endif

static u64				sata_dmamask = DMA_BIT_MASK(32);
static struct ahci_platform_data	sata_pdata;

#if (!defined(CONFIG_MACH_OMAP_5430ZEBU) && !defined(CONFIG_OMAP5_VIRTIO))
static struct clk			*sata_ref_clk;
#endif

static struct omap_device_pm_latency omap_sata_latency[] = {
	  {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	  },
};

static inline void omap_sata_writel(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 omap_sata_readl(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

#ifdef OMAP_SATA_PHY_PWR
static void sata_phy_pwr_on(void)
{
	void __iomem	*base;

	base = ioremap(OMAP_CTRL_MODULE_CORE, OMAP_CTRL_MODULE_CORE_SIZE);
	if (base) {
		omap_sata_writel(base, OMAP_CTRL_SATA_PHY_POWER,
				(SATA_PWRCTL_CLK_FREQ | SATA_PWRCTL_CLK_CMD));
		omap_sata_writel(base, OMAP_CTRL_SATA_EXT_MODE, 1);
	}
}
#endif

/*
 * - FIXME -
 * Following PLL configuration will be removed in future,
 * to a seperate platform driver
 */
static int sata_phy_init(struct device *dev)
{
#if (!defined(CONFIG_MACH_OMAP_5430ZEBU) && !defined(CONFIG_OMAP5_VIRTIO))
	void __iomem		*pll;
	struct resource		*res;
	struct platform_device	*pdev;
	u32			reg;
	int			ret;
	unsigned long		timeout;

	pdev =	container_of(dev, struct platform_device, dev);

	/*Enable the sata reference clock */
	sata_ref_clk = clk_get(dev, "ref_clk");
	if (IS_ERR(sata_ref_clk)) {
		ret = PTR_ERR(sata_ref_clk);
		dev_err(dev, "ref_clk failed:%d\n", ret);
		return ret;
	}
	clk_enable(sata_ref_clk);

#ifdef OMAP_SATA_PHY_PWR
	sata_phy_pwr_on();
#endif
	res =  platform_get_resource_byname(pdev, IORESOURCE_MEM, "pll");
	if (!res) {
		dev_err(dev, "pll get resource failed\n");
		return -ENODEV;
	}

	pll = ioremap(res->start, resource_size(res));
	if (!pll) {
		dev_err(dev, "can't map 0x%X\n", res->start);
		return -ENOMEM;
	}

	/* set the configuration 1; The phy clocks */
	omap_sata_writel(pll, OMAP_SATA_PLL_CONFIGURATION1,
				OMAP_SATA_PLL_CONFIGURATION1_1_5G);

	/* Enable phy clock */
	reg  = omap_sata_readl(pll, OMAP_SATA_PLL_CONFIGURATION2);
	reg |= OMAP_SATA_PLL_REF_CLK_ENABLE;
	omap_sata_writel(pll, OMAP_SATA_PLL_CONFIGURATION2, reg);

	omap_sata_writel(pll, OMAP_SATA_PLL_GO, 1);

	/* Poll for the PLL lock */
	timeout = jiffies + msecs_to_jiffies(1000);
	while (!(omap_sata_readl(pll, 0x4) & (1 << 1))) {
		cpu_relax();

		if (time_after(jiffies, timeout)) {
			dev_err(dev, "sata phy pll lock timed out\n");
			break;
		}
	}
	iounmap(pll);
#endif
	return 0;
}

static void sata_phy_exit(void)
{
#if (!defined(CONFIG_MACH_OMAP_5430ZEBU) && !defined(CONFIG_OMAP5_VIRTIO))
	clk_disable(sata_ref_clk);
	clk_put(sata_ref_clk);
#endif
}

static int omap_ahci_plat_init(struct device *dev, void __iomem *base)
{
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	sata_phy_init(dev);
	return 0;
}

static void omap_ahci_plat_exit(struct device *dev)
{
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	sata_phy_exit();
}

void __init omap_sata_init(void)
{
	struct omap_hwmod	*hwmod;
	struct omap_device	*od;
	struct platform_device	*pdev;
	struct device		*dev;

	/* For now sata init works only for omap5 */
	if (!cpu_is_omap54xx())
		return;

	sata_pdata.init		= omap_ahci_plat_init;
	sata_pdata.exit		= omap_ahci_plat_exit;

	hwmod = omap_hwmod_lookup(OMAP_SATA_HWMODNAME);
	if (!hwmod) {
		pr_err("Could not look up %s\n", OMAP_SATA_HWMODNAME);
		return;
	}

	od = omap_device_build(AHCI_PLAT_DEVNAME, -1, hwmod,
				(void *) &sata_pdata, sizeof(sata_pdata),
				omap_sata_latency,
				ARRAY_SIZE(omap_sata_latency), false);
	if (IS_ERR(od)) {
		pr_err("Could not build hwmod device %s\n",
					OMAP_SATA_HWMODNAME);
		return;
	}
	pdev = &od->pdev;
	dev = &pdev->dev;
	get_device(dev);
	dev->dma_mask = &sata_dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	put_device(dev);
}
#else

void __init omap_sata_init(void)
{
}

#endif
