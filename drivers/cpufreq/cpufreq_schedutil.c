/*
 * CPUFreq governor based on scheduler-provided CPU utilization data.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/percpu-defs.h>
#include <linux/slab.h>

#include "cpufreq_governor.h"

struct sugov_policy {
	struct policy_dbs_info policy_dbs;
	unsigned int next_freq;
	raw_spinlock_t update_lock;  /* For shared policies */
};

static inline struct sugov_policy *to_sg_policy(struct policy_dbs_info *policy_dbs)
{
	return container_of(policy_dbs, struct sugov_policy, policy_dbs);
}

struct sugov_cpu {
	struct update_util_data update_util;
	struct policy_dbs_info *policy_dbs;
	/* The fields below are only needed when sharing a policy. */
	unsigned long util;
	unsigned long max;
	u64 last_update;
};

static DEFINE_PER_CPU(struct sugov_cpu, sugov_cpu);

/************************ Governor internals ***********************/

static unsigned int sugov_next_freq(struct policy_dbs_info *policy_dbs,
				    unsigned long util, unsigned long max,
				    u64 last_sample_time)
{
	struct cpufreq_policy *policy = policy_dbs->policy;
	unsigned int min_f = policy->min;
	unsigned int max_f = policy->max;
	unsigned int j;

	if (util > max || min_f >= max_f)
		return max_f;

	for_each_cpu(j, policy->cpus) {
		struct sugov_cpu *j_sg_cpu;
		unsigned long j_util, j_max;

		if (j == smp_processor_id())
			continue;

		j_sg_cpu = &per_cpu(sugov_cpu, j);
		/*
		 * If the CPU was last updated before the previous sampling
		 * time, we don't take it into account as it probably is idle
		 * now.
		 */
		if (j_sg_cpu->last_update < last_sample_time)
			continue;

		j_util = j_sg_cpu->util;
		j_max = j_sg_cpu->max;
		if (j_util > j_max)
			return max_f;

		if (j_util * max > j_max * util) {
			util = j_util;
			max = j_max;
		}
	}

	return min_f + util * (max_f - min_f) / max;
}

static void sugov_update_commit(struct policy_dbs_info *policy_dbs, u64 time,
				unsigned int next_freq)
{
	struct sugov_policy *sg_policy = to_sg_policy(policy_dbs);

	policy_dbs->last_sample_time = time;
	if (sg_policy->next_freq == next_freq)
		return;

	sg_policy->next_freq = next_freq;
	if (policy_dbs->fast_switch_enabled) {
		cpufreq_driver_fast_switch(policy_dbs->policy, next_freq);
		/*
		 * Restore the sample delay in case it has been set to 0 from
		 * sysfs in the meantime.
		 */
		gov_update_sample_delay(policy_dbs,
					policy_dbs->dbs_data->sampling_rate);
	} else {
		policy_dbs->work_in_progress = true;
		irq_work_queue(&policy_dbs->irq_work);
	}
}

static void sugov_update_shared(struct update_util_data *data, u64 time,
				unsigned long util, unsigned long max)
{
	struct sugov_cpu *sg_cpu = container_of(data, struct sugov_cpu, update_util);
	struct policy_dbs_info *policy_dbs = sg_cpu->policy_dbs;
	struct sugov_policy *sg_policy = to_sg_policy(policy_dbs);
	unsigned int next_f;
	u64 delta_ns, lst;

	raw_spin_lock(&sg_policy->update_lock);

	sg_cpu->util = util;
	sg_cpu->max = max;
	sg_cpu->last_update = time;

	if (policy_dbs->work_in_progress)
		goto out;

	/*
	 * This assumes that dbs_data_handler() will not change sample_delay_ns.
	 */
	lst = policy_dbs->last_sample_time;
	delta_ns = time - lst;
	if ((s64)delta_ns < policy_dbs->sample_delay_ns)
		goto out;

	next_f = sugov_next_freq(policy_dbs, util, max, lst);

	sugov_update_commit(policy_dbs, time, next_f);

 out:
	raw_spin_unlock(&sg_policy->update_lock);
}

static void sugov_update_single(struct update_util_data *data, u64 time,
				unsigned long util, unsigned long max)
{
	struct sugov_cpu *sg_cpu = container_of(data, struct sugov_cpu, update_util);
	struct policy_dbs_info *policy_dbs = sg_cpu->policy_dbs;
	unsigned int min_f, max_f, next_f;
	u64 delta_ns;

	if (policy_dbs->work_in_progress)
		return;

	/*
	 * This assumes that dbs_data_handler() will not change sample_delay_ns.
	 */
	delta_ns = time - policy_dbs->last_sample_time;
	if ((s64)delta_ns < policy_dbs->sample_delay_ns)
		return;

	min_f = policy_dbs->policy->min;
	max_f = policy_dbs->policy->max;
	next_f = util > max || min_f >= max_f ? max_f :
			min_f + util * (max_f - min_f) / max;

	sugov_update_commit(policy_dbs, time, next_f);
}

/************************** sysfs interface ************************/

gov_show_one_common(sampling_rate);

gov_attr_rw(sampling_rate);

static struct attribute *sugov_attributes[] = {
	&sampling_rate.attr,
	NULL
};

/********************** dbs_governor interface *********************/

static struct policy_dbs_info *sugov_alloc(void)
{
	struct sugov_policy *sg_policy;

	sg_policy = kzalloc(sizeof(*sg_policy), GFP_KERNEL);
	if (!sg_policy)
		return NULL;

	raw_spin_lock_init(&sg_policy->update_lock);
	return &sg_policy->policy_dbs;
}

static void sugov_free(struct policy_dbs_info *policy_dbs)
{
	kfree(to_sg_policy(policy_dbs));
}

static bool sugov_start(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct sugov_policy *sg_policy = to_sg_policy(policy_dbs);
	unsigned int cpu;

	gov_update_sample_delay(policy_dbs, policy_dbs->dbs_data->sampling_rate);
	policy_dbs->last_sample_time = 0;
	policy_dbs->fast_switch_enabled = policy->fast_switch_possible;
	sg_policy->next_freq = UINT_MAX;

	for_each_cpu(cpu, policy->cpus) {
		struct sugov_cpu *sg_cpu = &per_cpu(sugov_cpu, cpu);

		sg_cpu->policy_dbs = policy_dbs;
		if (policy_dbs->is_shared) {
			sg_cpu->util = ULONG_MAX;
			sg_cpu->max = 0;
			sg_cpu->last_update = 0;
			sg_cpu->update_util.func = sugov_update_shared;
		} else {
			sg_cpu->update_util.func = sugov_update_single;
		}
		cpufreq_set_update_util_data(cpu, &sg_cpu->update_util);
	}
	return false;
}

static unsigned int sugov_set_freq(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct sugov_policy *sg_policy = to_sg_policy(policy_dbs);

	__cpufreq_driver_target(policy, sg_policy->next_freq, CPUFREQ_RELATION_C);
	return policy_dbs->dbs_data->sampling_rate;
}

static struct dbs_governor schedutil_gov = {
	.gov = {
		.name = "schedutil",
		.governor = cpufreq_governor_dbs,
		.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
		.owner = THIS_MODULE,
	},
	.kobj_type = { .default_attrs = sugov_attributes },
	.gov_dbs_timer = sugov_set_freq,
	.alloc = sugov_alloc,
	.free = sugov_free,
	.start = sugov_start,
};

#define CPU_FREQ_GOV_SCHEDUTIL	(&schedutil_gov.gov)

static int __init sugov_init(void)
{
	return cpufreq_register_governor(CPU_FREQ_GOV_SCHEDUTIL);
}

static void __exit sugov_exit(void)
{
	cpufreq_unregister_governor(CPU_FREQ_GOV_SCHEDUTIL);
}

MODULE_AUTHOR("Rafael J. Wysocki <rafael.j.wysocki@intel.com>");
MODULE_DESCRIPTION("Utilization-based CPU frequency selection");
MODULE_LICENSE("GPL");

module_init(sugov_init);
module_exit(sugov_exit);
