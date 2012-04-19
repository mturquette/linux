#include <linux/kernel.h>
#include <linux/clk-private.h>
//#include <linux/opp.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#include <plat/omap_device.h>

static struct clk *mpu_clk;
static struct device *mpu_dev;
static struct regulator *mpu_reg;

#define	OPP_TOLERANCE	4

#define OMAP4430_VDD_MPU_OPP50_UV		1025000
#define OMAP4430_VDD_MPU_OPP100_UV		1200000
#define OMAP4430_VDD_MPU_OPPTURBO_UV		1313000
#define OMAP4430_VDD_MPU_OPPNITRO_UV		1375000

static int dvfs_scale_volt_mpu(struct notifier_block *nb,
	unsigned long flags, void *data)
{
	struct clk_notifier_data *cnd = data;
	unsigned long tol;
	int ret, volt;

	pr_err("%s: clk->name is %s, flags are %lu\n",
			__func__, cnd->clk->name, flags);

	pr_err("%s: new_rate is %lu, old_rate is %lu\n",
			__func__, cnd->new_rate, cnd->old_rate);

	if (flags == PRE_RATE_CHANGE && cnd->new_rate < cnd->old_rate) {
		pr_err("%s: PRE_RATE_CHANGE + scaling down: do nothing\n", __func__);
		return 0;
	}

	if (flags == POST_RATE_CHANGE && cnd->new_rate > cnd->old_rate) {
		pr_err("%s: POST_RATE_CHANGE + scaling up: do nothing\n", __func__);
		return 0;
	}

	/* hacky table lookup */
	if (cnd->new_rate <= 300000000) {
		volt = OMAP4430_VDD_MPU_OPP50_UV;
		tol = volt * OPP_TOLERANCE / 100;

		pr_err("%s: rate is <= 300000000\n", __func__);
		pr_err("%s: old voltage is %d, new voltage is %d\n",
				__func__,
				regulator_get_voltage(mpu_reg),
				volt);

		ret = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
	} else if (cnd->new_rate <= 600000000) {
		volt = OMAP4430_VDD_MPU_OPP100_UV;
		tol = volt * OPP_TOLERANCE / 100;

		pr_err("%s: rate is <= 600000000\n", __func__);
		pr_err("%s: old voltage is %d, new voltage is %d\n",
				__func__,
				regulator_get_voltage(mpu_reg),
				volt);

		ret = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
	} else if (cnd->new_rate <= 800000000) {
		volt = OMAP4430_VDD_MPU_OPPTURBO_UV;
		tol = volt * OPP_TOLERANCE / 100;

		pr_err("%s: rate is <= 800000000\n", __func__);
		pr_err("%s: old voltage is %d, new voltage is %d\n",
				__func__,
				regulator_get_voltage(mpu_reg),
				volt);

		ret = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
	} else if (cnd->new_rate <= 1000800000) {
		volt = OMAP4430_VDD_MPU_OPPNITRO_UV;
		tol = volt * OPP_TOLERANCE / 100;

		pr_err("%s: rate is <= 1000800000\n", __func__);
		pr_err("%s: old voltage is %d, new voltage is %d\n",
				__func__,
				regulator_get_voltage(mpu_reg),
				volt);

		ret = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
	} else {
		pr_err("%s: rate is insane\n", __func__);
	}
	
#if 0
	if (mpu_reg) {
		opp = opp_find_freq_ceil(mpu_dev, &freq);
		if (IS_ERR(opp)) {
			dev_err(mpu_dev, "%s: unable to find MPU OPP for %d\n",
				__func__, freqs.new);
			return -EINVAL;
		}
		volt = opp_get_voltage(opp);
		tol = volt * OPP_TOLERANCE / 100;
		volt_old = regulator_get_voltage(mpu_reg);
	}

	dev_dbg(mpu_dev, "cpufreq-omap: %u MHz, %ld mV --> %u MHz, %ld mV\n", 
		freqs.old / 1000, volt_old ? volt_old / 1000 : -1,
		freqs.new / 1000, volt ? volt / 1000 : -1);

	/* scaling up?  scale voltage before frequency */
	if (mpu_reg && (freqs.new > freqs.old)) {
		r = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
		if (r < 0) {
			dev_warn(mpu_dev, "%s: unable to scale voltage up.\n",
				 __func__);
			freqs.new = freqs.old;
			goto done;
		}
	}

	//clk_set_rate(...)

	/* scaling down?  scale voltage after frequency */
	if (mpu_reg && (freqs.new < freqs.old)) {
		r = regulator_set_voltage(mpu_reg, volt - tol, volt + tol);
		if (r < 0) {
			dev_warn(mpu_dev, "%s: unable to scale voltage down.\n",
				 __func__);
			ret = clk_set_rate(mpu_clk, freqs.old * 1000);
			freqs.new = freqs.old;
			goto done;
		}
	}
#endif

	return NOTIFY_OK;
}

static struct notifier_block dvfs_clk_mpu_nb = {
	.notifier_call = dvfs_scale_volt_mpu,
};

#if 0
static int dvfs_scale_volt_core(struct notifier_block *nb,
	unsigned long flags, void *data)
{
	struct clk_notifier_data *cnd = data;

	if (flags == PRE_RATE_CHANGE && cnd->new_rate > cnd->old_rate) {
		regulator_set_voltage();
	}
	
	if (flags == POST_RATE_CHANGE && cnd->new_rate < cnd->old_rate) {
		regulator_set_voltage();
	}

	return NOTIFY_OK;
}

static struct notifier_block dvfs_clk_core_nb = {
	.notifier_call = dvfs_scale_volt_core,
};

static int __init dvfs_core_reg_init(void)
{
	/* FIXME
	 * would be cool to do something like:
	 * core_clk = clk_get(core_dev, "fck");
	 */
	core_clk = clk_get(NULL, "l3_div_ck");
	//core_clk = clk_get(

	if (!core_clk_name) {
		pr_err("%s: unsupported Silicon?\n", __func__);
		return -EINVAL;
	}

	core_dev = omap_device_get_by_hwmod_name("dma_system");
	if (!core_dev) {
		pr_warning("%s: unable to get the core device\n", __func__);
		return -EINVAL;
	}

	core_reg = regulator_get(core_dev, "vcc");
	if (IS_ERR(core_reg)) {
		pr_warning("%s: unable to get CORE regulator\n", __func__);
		core_reg = NULL;
	} else {
		/* 
		 * Ensure physical regulator is present.
		 * (e.g. could be dummy regulator.)
		 */
		if (regulator_get_voltage(core_reg) < 0) {
			pr_warn("%s: physical regulator not present for CORE\n",
				__func__);
			regulator_put(core_reg);
			core_reg = NULL;
		}
	}

	return 0;
}
#endif

static int __init dvfs_mpu_reg_init(void)
{
	mpu_clk = clk_get(NULL, "dpll_mpu_m2_ck");

#if 0
	if (!mpu_clk_name) {
		pr_err("%s: unsupported Silicon?\n", __func__);
		return -EINVAL;
	}
#endif

	mpu_dev = omap_device_get_by_hwmod_name("mpu");
	if (!mpu_dev) {
		pr_warning("%s: unable to get the mpu device\n", __func__);
		return -EINVAL;
	}

	mpu_reg = regulator_get(mpu_dev, "vcc");
	if (IS_ERR(mpu_reg)) {
		pr_warning("%s: unable to get MPU regulator\n", __func__);
		mpu_reg = NULL;
	} else {
		/* 
		 * Ensure physical regulator is present.
		 * (e.g. could be dummy regulator.)
		 */
		if (regulator_get_voltage(mpu_reg) < 0) {
			pr_warn("%s: physical regulator not present for MPU\n",
				__func__);
			regulator_put(mpu_reg);
			mpu_reg = NULL;
		}
	}

	return 0;
}

static int __init dvfs_init(void)
{
	int ret;

#if 0
	/* FIXME - clkdev should expose a single "mpu_clk" */
	if (cpu_is_omap24xx())
		mpu_clk_name = "virt_prcm_set";
	else if (cpu_is_omap34xx())
		mpu_clk_name = "dpll1_ck";
	else if (cpu_is_omap44xx())
		mpu_clk_name = "mpu_clk";
#endif

#if 0
	ret = dvfs_core_reg_init();
	if (ret)
		pr_err("%s: dvfs_core_reg_init failed\n");
#endif

	ret = dvfs_mpu_reg_init();
	if (ret)
		pr_err("%s: dvfs_mpu_reg_init failed\n", __func__);

	return clk_notifier_register(mpu_clk, &dvfs_clk_mpu_nb);
}
late_initcall(dvfs_init);
