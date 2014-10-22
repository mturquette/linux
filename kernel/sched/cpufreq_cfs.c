/*
 *  Copyright (C)  2015 Michael Turquette <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/percpu.h>
#include <linux/irq_work.h>

#include "sched.h"

#define MARGIN_PCT		125 /* taken from imbalance_pct = 125 */
#define THROTTLE_NSEC		50000000 /* 50ms default */

static DEFINE_PER_CPU(unsigned long, pcpu_util);
static DEFINE_PER_CPU(struct cpufreq_policy *, pcpu_policy);

/**
 * gov_data - per-policy data internal to the governor
 * @throttle: next throttling period expiry. Derived from throttle_nsec
 * @throttle_nsec: throttle period length in nanoseconds
 * @task: worker thread for dvfs transition that may block/sleep
 * @irq_work: callback used to wake up worker thread
 * @freq: new frequency stored in *_cfs_update_cpu and used in *_cfs_thread
 *
 * struct gov_data is the per-policy cpufreq_cfs-specific data structure. A
 * per-policy instance of it is created when the cpufreq_cfs governor receives
 * the CPUFREQ_GOV_START condition and a pointer to it exists in the gov_data
 * member of struct cpufreq_policy.
 *
 * Readers of this data must call down_read(policy->rwsem). Writers must
 * call down_write(policy->rwsem).
 */
struct gov_data {
	ktime_t throttle;
	unsigned int throttle_nsec;
	struct task_struct *task;
	struct irq_work irq_work;
	struct cpufreq_policy *policy;
	unsigned int freq;
};

/*
 * we pass in struct cpufreq_policy. This is safe because changing out the
 * policy requires a call to __cpufreq_governor(policy, CPUFREQ_GOV_STOP),
 * which tears down all of the data structures and __cpufreq_governor(policy,
 * CPUFREQ_GOV_START) will do a full rebuild, including this kthread with the
 * new policy pointer
 */
static int cpufreq_cfs_thread(void *data)
{
	struct sched_param param;
	struct cpufreq_policy *policy;
	struct gov_data *gd;
	int ret;

	policy = (struct cpufreq_policy *) data;
	if (!policy) {
		pr_warn("%s: missing policy\n", __func__);
		do_exit(-EINVAL);
	}

	gd = policy->governor_data;
	if (!gd) {
		pr_warn("%s: missing governor data\n", __func__);
		do_exit(-EINVAL);
	}

	param.sched_priority = 50;
	ret = sched_setscheduler_nocheck(gd->task, SCHED_FIFO, &param);
	if (ret) {
		pr_warn("%s: failed to set SCHED_FIFO\n", __func__);
		do_exit(-EINVAL);
	} else {
		pr_debug("%s: kthread (%d) set to SCHED_FIFO\n",
				__func__, gd->task->pid);
	}

	ret = set_cpus_allowed_ptr(gd->task, policy->related_cpus);
	if (ret) {
		pr_warn("%s: failed to set allowed ptr\n", __func__);
		do_exit(-EINVAL);
	}

	/* main loop of the per-policy kthread */
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if (kthread_should_stop())
			break;

		/* avoid race with cpufreq_cfs_stop */
		if (!down_write_trylock(&policy->rwsem))
			continue;

		ret = __cpufreq_driver_target(policy, gd->freq,
				CPUFREQ_RELATION_L);
		if (ret)
			pr_debug("%s: __cpufreq_driver_target returned %d\n",
					__func__, ret);

		gd->throttle = ktime_add_ns(ktime_get(), gd->throttle_nsec);
		up_write(&policy->rwsem);
	} while (!kthread_should_stop());

	do_exit(0);
}

static void cpufreq_cfs_irq_work(struct irq_work *irq_work)
{
	struct gov_data *gd;

	gd = container_of(irq_work, struct gov_data, irq_work);
	if (!gd) {
		return;
	}

	wake_up_process(gd->task);
}

/**
 * cpufreq_cfs_update_cpu - interface to scheduler for changing capacity values
 * @cpu: cpu whose capacity utilization has recently changed
 *
 * cpufreq_cfs_update_cpu is an interface exposed to the scheduler so that the
 * scheduler may inform the governor of updates to capacity utilization and
 * make changes to cpu frequency. Currently this interface is designed around
 * PELT values in CFS. It can be expanded to other scheduling classes in the
 * future if needed.
 *
 * cpufreq_cfs_update_cpu raises an IPI. The irq_work handler for that IPI wakes up
 * the thread that does the actual work, cpufreq_cfs_thread.
 *
 * This functions bails out early if either condition is true:
 * 1) this cpu is not the new maximum utilization for its frequency domain
 * 2) no change in cpu frequency is necessary to meet the new capacity request
 *
 * Returns the newly chosen capacity. Note that this may not reflect reality if
 * the hardware fails to transition to this new capacity state.
 */
unsigned long cpufreq_cfs_update_cpu(int cpu, unsigned long util)
{
	unsigned long util_new, util_old, util_max, capacity_new;
	unsigned int freq_new, freq_tmp, cpu_tmp;
	struct cpufreq_policy *policy;
	struct gov_data *gd;
	struct cpufreq_frequency_table *pos;

	/* handle rounding errors */
	util_new = util > SCHED_LOAD_SCALE ? SCHED_LOAD_SCALE : util;

	/* update per-cpu utilization */
	util_old = __this_cpu_read(pcpu_util);
	__this_cpu_write(pcpu_util, util_new);

	/* avoid locking policy for now; accessing .cpus only */
	policy = per_cpu(pcpu_policy, cpu);

	/* find max utilization of cpus in this policy */
	util_max = 0;
	for_each_cpu(cpu_tmp, policy->cpus)
		util_max = max(util_max, per_cpu(pcpu_util, cpu_tmp));

	/*
	 * We only change frequency if this cpu's utilization represents a new
	 * max. If another cpu has increased its utilization beyond the
	 * previous max then we rely on that cpu to hit this code path and make
	 * the change. IOW, the cpu with the new max utilization is responsible
	 * for setting the new capacity/frequency.
	 *
	 * If this cpu is not the new maximum then bail, returning the current
	 * capacity.
	 */
	if (util_max > util_new)
		return capacity_of(cpu);

	/*
	 * We are going to request a new capacity, which might result in a new
	 * cpu frequency. From here on we need to serialize access to the
	 * policy and the governor private data.
	 */
	policy = cpufreq_cpu_get(cpu);
	if (IS_ERR_OR_NULL(policy)) {
		return capacity_of(cpu);
	}

	capacity_new = capacity_of(cpu);
	if (!policy->governor_data) {
		goto out;
	}

	gd = policy->governor_data;

	/* bail early if we are throttled */
	if (ktime_before(ktime_get(), gd->throttle)) {
		goto out;
	}

	/*
	 * Convert the new maximum capacity utilization into a cpu frequency
	 *
	 * It is possible to convert capacity utilization directly into a
	 * frequency, but that implies that we would be 100% utilized. Instead,
	 * first add a margin (default 25% capacity increase) to the new
	 * capacity request. This provides some head room if load increases.
	 */
	capacity_new = util_new + (SCHED_CAPACITY_SCALE >> 2);
	freq_new = capacity_new * policy->max >> SCHED_CAPACITY_SHIFT;

	/*
	 * If a frequency table is available then find the frequency
	 * corresponding to freq_new.
	 *
	 * For cpufreq drivers without a frequency table, use the frequency
	 * directly computed from capacity_new + 25% margin.
	 */
	if (policy->freq_table) {
		freq_tmp = policy->max;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency >= freq_new &&
					pos->frequency < freq_tmp)
				freq_tmp = pos->frequency;
		}
		freq_new = freq_tmp;
		capacity_new = (freq_new << SCHED_CAPACITY_SHIFT) / policy->max;
	}

	/* No change in frequency? Bail and return current capacity. */
	if (freq_new == policy->cur) {
		capacity_new = capacity_of(cpu);
		goto out;
	}

	/* store the new frequency and kick the thread */
	gd->freq = freq_new;

	/* XXX can we use something like try_to_wake_up_local here instead? */
	irq_work_queue_on(&gd->irq_work, cpu);

out:
	cpufreq_cpu_put(policy);
	return capacity_new;
}

static void cpufreq_cfs_start(struct cpufreq_policy *policy)
{
	struct gov_data *gd;
	int cpu;

	/* prepare per-policy private data */
	gd = kzalloc(sizeof(*gd), GFP_KERNEL);
	if (!gd) {
		pr_debug("%s: failed to allocate private data\n", __func__);
		return;
	}

	/* initialize per-cpu data */
	for_each_cpu(cpu, policy->cpus) {
		per_cpu(pcpu_util, cpu) = 0;
		per_cpu(pcpu_policy, cpu) = policy;
	}

	/*
	 * Don't ask for freq changes at an higher rate than what
	 * the driver advertises as transition latency.
	 */
	gd->throttle_nsec = policy->cpuinfo.transition_latency ?
			    policy->cpuinfo.transition_latency :
			    THROTTLE_NSEC;
	pr_debug("%s: throttle threshold = %u [ns]\n",
		  __func__, gd->throttle_nsec);

	/* init per-policy kthread */
	gd->task = kthread_run(cpufreq_cfs_thread, policy, "kcpufreq_cfs_task");
	if (IS_ERR_OR_NULL(gd->task))
		pr_err("%s: failed to create kcpufreq_cfs_task thread\n", __func__);

	init_irq_work(&gd->irq_work, cpufreq_cfs_irq_work);
	policy->governor_data = gd;
	gd->policy = policy;
}

static void cpufreq_cfs_stop(struct cpufreq_policy *policy)
{
	struct gov_data *gd;

	gd = policy->governor_data;
	kthread_stop(gd->task);

	policy->governor_data = NULL;

	/* FIXME replace with devm counterparts? */
	kfree(gd);
}

static int cpufreq_cfs_setup(struct cpufreq_policy *policy, unsigned int event)
{
	switch (event) {
		case CPUFREQ_GOV_START:
			/* Start managing the frequency */
			cpufreq_cfs_start(policy);
			return 0;

		case CPUFREQ_GOV_STOP:
			cpufreq_cfs_stop(policy);
			return 0;

		case CPUFREQ_GOV_LIMITS:	/* unused */
		case CPUFREQ_GOV_POLICY_INIT:	/* unused */
		case CPUFREQ_GOV_POLICY_EXIT:	/* unused */
			break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SCHED_CFS
static
#endif
struct cpufreq_governor cpufreq_cfs = {
	.name			= "cfs",
	.governor		= cpufreq_cfs_setup,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_cfs_init(void)
{
	return cpufreq_register_governor(&cpufreq_cfs);
}

static void __exit cpufreq_cfs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_cfs);
}

/* Try to make this the default governor */
fs_initcall(cpufreq_cfs_init);

MODULE_LICENSE("GPL");
