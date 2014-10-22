/*
 *  Copyright (C)  2014 Michael Turquette <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/percpu.h>

#include "sched.h"

#define UP_THRESHOLD		95
#define THROTTLE_NSEC		50000000

/* XXX always true, for now */
static bool driver_might_sleep = true;

/*
 * per-cpu pointer to atomic_t gov_data->cap_gov_wake_task
 * used in scheduler hot paths {en,de}queueu, task_tick without having to
 * access struct cpufreq_policy and struct gov_data
 */
static DEFINE_PER_CPU(atomic_t *, cap_gov_wake_task);

/*
 * FIXME if we move to an arbitrator model then we can have a single thread for
 * the whole system. This means that per-policy data can be removed and
 * replaced with per-governor data.
 *
 * FIXME correction to the above statement. The thread doesn't matter. We could
 * always get rid of per-policy data by having per-cpu pointers to some private
 * data. This is how the legacy governors do it. I'm not sure its any better or
 * worse.
 */

/**
 * gov_data - per-policy data internal to the governor
 * @throttle: time until throttling period expires. Derived from THROTTLE_NSEC
 * @task: worker task for dvfs transition that may block/sleep
 * @need_wake_task: flag the governor to wake this policy's worker thread
 *
 * struct gov_data is the per-policy cap_gov-specific data structure. A
 * per-policy instance of it is created when the cap_gov governor receives
 * the CPUFREQ_GOV_START condition and a pointer to it exists in the gov_data
 * member of struct cpufreq_policy.
 *
 * Readers of this data must call down_read(policy->rwsem). Writers must
 * call down_write(policy->rwsem).
 */
struct gov_data {
	ktime_t throttle;
	struct task_struct *task;
	atomic_t need_wake_task;
};

/**
 * cap_gov_select_freq - pick the next frequency for a cpu
 * @cpu: the cpu whose frequency may be changed
 *
 * cap_gov_select_freq works in a way similar to the ondemand governor. First
 * we inspect the utilization of all of the cpus in this policy to find the
 * most utilized cpu. This is achieved by calling get_cpu_usage, which returns
 * frequency-invarant capacity utilization.
 *
 * This max utilization is compared against the up_threshold (default 95%
 * utilization). If the max cpu utilization is greater than this threshold then
 * we scale the policy up to the max frequency. Othewise we find the lowest
 * frequency (smallest cpu capacity) that is still larger than the max capacity
 * utilization for this policy.
 *
 * Returns frequency selected.
 */
static unsigned long cap_gov_select_freq(struct cpufreq_policy *policy)
{
	int cpu = 0;
	struct gov_data *gd;
	int index;
	unsigned long freq = 0, max_usage = 0, cap = 0, usage = 0;
	struct cpufreq_frequency_table *pos;

	if (!policy->gov_data)
		goto out;

	gd = policy->gov_data;

	/*
	 * get_cpu_usage is called without locking the runqueues. This is the
	 * same behavior used by find_busiest_cpu in load_balance. We are
	 * willing to accept occasionally stale data here in exchange for
	 * lockless behavior.
	 */
	for_each_cpu(cpu, policy->cpus) {
		usage = get_cpu_usage(cpu);
		trace_printk("cpu = %d usage = %lu", cpu, usage);
		if (usage > max_usage)
			max_usage = usage;
	}
	trace_printk("max_usage = %lu", max_usage);

	/* find the utilization threshold at which we scale up frequency */
	index = cpufreq_frequency_table_get_index(policy, policy->cur);

	/*
	 * converge towards max_usage. We want the lowest frequency whose
	 * capacity is >= to max_usage. In other words:
	 *
	 * 	find capacity == floor(usage)
	 *
	 * Sadly cpufreq freq tables are not guaranteed to be ordered by
	 * frequency...
	 */
	freq = policy->max;
	cpufreq_for_each_entry(pos, policy->freq_table) {
		cap = pos->frequency * SCHED_CAPACITY_SCALE /
			policy->max;
		if (max_usage < cap && pos->frequency < freq)
			freq = pos->frequency;
		trace_printk("cpu = %u max_usage = %lu cap = %lu \
				table_freq = %u freq = %lu",
				cpumask_first(policy->cpus), max_usage, cap,
				pos->frequency, freq);
	}

out:
	trace_printk("cpu %d final freq %lu", cpu, freq);
	return freq;
}

/**
 * cap_gov_target_freq - transition cpu to selected frequency
 * @cpu: the cpu whose frequency may be changed
 *
 * Caller must hold cpufreq_policy rwsem lock.
 *
 * cap_gov_select_freq works in a way similar to the ondemand governor. First
 * we inspect the utilization of all of the cpus in this policy to find the
 * most utilized cpu. This is achieved by calling get_cpu_usage, which returns
 * frequency-invarant capacity utilization.
 *
 * This max utilization is compared against the up_threshold (default 95%
 * utilization). If the max cpu utilization is greater than this threshold then
 * we scale the policy up to the max frequency. Othewise we find the lowest
 * frequency (smallest cpu capacity) that is still larger than the max capacity
 * utilization for this policy.
 *
 * Returns frequency selected.
 */
static int cap_gov_target_freq(struct cpufreq_policy *policy)
{
	struct gov_data *gd;
	int ret;
	unsigned int freq;

	gd = policy->gov_data;
	if (!gd) {
		ret = -EINVAL;
		goto out;
	}

	freq = cap_gov_select_freq(policy);

	ret = __cpufreq_driver_target(policy, freq,
			CPUFREQ_RELATION_H);
	if (ret)
		pr_debug("%s: __cpufreq_driver_target returned %d\n",
				__func__, ret);

	gd->throttle = ktime_add_ns(ktime_get(), THROTTLE_NSEC);

out:
	return ret;
}

/*
 * we pass in struct cpufreq_policy. This is safe because changing out the
 * policy requires a call to __cpufreq_governor(policy, CPUFREQ_GOV_STOP),
 * which tears down all of the data structures and __cpufreq_governor(policy,
 * CPUFREQ_GOV_START) will do a full rebuild, including this kthread with the
 * new policy pointer
 */
static int cap_gov_thread(void *data)
{
	struct sched_param param;
	struct cpufreq_policy *policy;
	struct gov_data *gd;
	unsigned long freq;
	int ret;

	policy = (struct cpufreq_policy *) data;
	if (!policy) {
		pr_warn("%s: missing policy\n", __func__);
		do_exit(-EINVAL);
	}

	gd = policy->gov_data;
	if (!gd) {
		pr_warn("%s: missing governor data\n", __func__);
		do_exit(-EINVAL);
	}

	param.sched_priority = 0;
	sched_setscheduler(current, SCHED_FIFO, &param);
	set_cpus_allowed_ptr(current, policy->related_cpus);

	/* main loop of the per-policy kthread */
	do {
		down_write(&policy->rwsem);
		if (!atomic_read(&gd->need_wake_task))  {
			trace_printk("NOT waking up kthread (%d)", gd->task->pid);
			up_write(&policy->rwsem);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}

		trace_printk("kthread %d requested freq switch", gd->task->pid);

#if 1
		freq = cap_gov_select_freq(policy);

		ret = __cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_H);
		if (ret)
			pr_debug("%s: __cpufreq_driver_target returned %d\n",
					__func__, ret);

		gd->throttle = ktime_add_ns(ktime_get(), THROTTLE_NSEC);
#else
		ret = cap_gov_target_freq(policy);
#endif
		atomic_set(&gd->need_wake_task, 0);
		up_write(&policy->rwsem);
	} while (!kthread_should_stop());

	do_exit(0);
}

static void cap_gov_wake_up_process(struct task_struct *task)
{
	/* this is null during early boot */
	if (IS_ERR_OR_NULL(task)) {
		return;
	}

	wake_up_process(task);
}

/* XXX possibly replace with Juri's method */
void cap_gov_kick_thread(int cpu)
{
	struct cpufreq_policy *policy;
	struct gov_data *gd = NULL;

	policy = cpufreq_cpu_get(cpu);
	if (IS_ERR_OR_NULL(policy))
		return;

	gd = policy->gov_data;
	if (!gd)
		goto out;

	/* per-cpu access not needed here since we have gd */
	if (atomic_read(&gd->need_wake_task)) {
		trace_printk("waking up kthread (%d)", gd->task->pid);
		cap_gov_wake_up_process(gd->task);
	}

out:
	cpufreq_cpu_put(policy);
}

/**
 * cap_gov_update_cpu - interface to scheduler for changing capacity values
 * @cpu: cpu whose capacity utilization has recently changed
 *
 * cap_gov_udpate_cpu is an interface exposed to the scheduler so that the
 * scheduler may inform the governor of updates to capacity utilization and
 * make changes to cpu frequency. Currently this interface is designed around
 * PELT values in CFS. It can be expanded to other scheduling classes in the
 * future if needed.
 *
 * The semantics of this call vary based on the cpu frequency scaling
 * characteristics of the hardware.
 *
 * If kicking off a dvfs transition is an operation that might block or sleep
 * in the cpufreq driver then we set the need_wake_task flag in this function
 * and return. Selecting a frequency and programming it is done in a dedicated
 * kernel thread which will be woken up from rebalance_domains. See
 * cap_gov_kick_thread above.
 *
 * If kicking off a dvfs transition is an operation that returns quickly in the
 * cpufreq driver and will never sleep then we select the frequency in this
 * function and program the hardware for it in the scheduler hot path. No
 * dedicated kthread is needed.
 */
void cap_gov_update_cpu(int cpu)
{
	struct cpufreq_policy *policy;
	struct gov_data *gd;

	/* XXX put policy pointer in per-cpu data? */
	policy = cpufreq_cpu_get(cpu);
	if (IS_ERR_OR_NULL(policy)) {
		return;
	}

	if (!policy->gov_data) {
		trace_printk("missing governor data");
		goto out;
	}

	gd = policy->gov_data;

	/* bail early if we are throttled */
	if (ktime_before(ktime_get(), gd->throttle)) {
		trace_printk("THROTTLED");
		goto out;
	}

	/* XXX driver_might_sleep is always true */
	if (driver_might_sleep) {
		atomic_set(per_cpu(cap_gov_wake_task, cpu), 1);
	} else {
		trace_printk("should not be here");
	}

out:
	cpufreq_cpu_put(policy);
	return;
}

static void cap_gov_start(struct cpufreq_policy *policy)
{
	int cpu;
	struct gov_data *gd;

	/* prepare per-policy private data */
	gd = kzalloc(sizeof(*gd), GFP_KERNEL);
	if (!gd) {
		pr_debug("%s: failed to allocate private data\n", __func__);
		return;
	}

	/* save per-cpu pointer to per-policy need_wake_task */
	for_each_cpu(cpu, policy->related_cpus)
		per_cpu(cap_gov_wake_task, cpu) = &gd->need_wake_task;

	/* FIXME if we move to an arbitrator model then we will only want one thread? */
	/* init per-policy kthread */
	gd->task = kthread_create(cap_gov_thread, policy, "kcap_gov_task");
	if (IS_ERR_OR_NULL(gd->task))
		pr_err("%s: failed to create kcap_gov_task thread\n", __func__);

	policy->gov_data = gd;
}

static void cap_gov_stop(struct cpufreq_policy *policy)
{
	struct gov_data *gd;

	gd = policy->gov_data;
	policy->gov_data = NULL;

	kthread_stop(gd->task);

	/* FIXME replace with devm counterparts? */
	kfree(gd);
}

static int cap_gov_setup(struct cpufreq_policy *policy, unsigned int event)
{
	switch (event) {
		case CPUFREQ_GOV_START:
			/* Start managing the frequency */
			cap_gov_start(policy);
			return 0;

		case CPUFREQ_GOV_STOP:
			cap_gov_stop(policy);
			return 0;

		case CPUFREQ_GOV_LIMITS:	/* unused */
		case CPUFREQ_GOV_POLICY_INIT:	/* unused */
		case CPUFREQ_GOV_POLICY_EXIT:	/* unused */
			break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_CAP_GOV
static
#endif
struct cpufreq_governor cpufreq_gov_cap_gov = {
	.name			= "cap_gov",
	.governor		= cap_gov_setup,
	.owner			= THIS_MODULE,
};

static int __init cap_gov_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_cap_gov);
}

static void __exit cap_gov_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_cap_gov);
}

/* Try to make this the default governor */
fs_initcall(cap_gov_init);

MODULE_LICENSE("GPL");
