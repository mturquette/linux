/*
 * Scheduler code and data structures related to cpufreq.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sched.h"

static DEFINE_PER_CPU(struct freq_update_hook *, cpufreq_freq_update_hook);

/**
 * set_freq_update_hook - Populate the CPU's freq_update_hook pointer.
 * @cpu: The CPU to set the pointer for.
 * @hook: New pointer value.
 *
 * Set and publish the freq_update_hook pointer for the given CPU.  That pointer
 * points to a struct freq_update_hook object containing a callback function
 * to call from cpufreq_update_util().  That function will be called from an
 * RCU read-side critical section, so it must not sleep.
 *
 * Callers must use RCU-sched callbacks to free any memory that might be
 * accessed via the old update_util_data pointer or invoke synchronize_sched()
 * right after this function to avoid use-after-free.
 */
static void set_freq_update_hook(int cpu, struct freq_update_hook *hook)
{
	rcu_assign_pointer(per_cpu(cpufreq_freq_update_hook, cpu), hook);
}

/**
 * cpufreq_set_freq_update_hook - Set the CPU's frequency update callback.
 * @cpu: The CPU to set the callback for.
 * @hook: New freq_update_hook pointer value.
 * @func: Callback function to use with the new hook.
 */
void cpufreq_set_freq_update_hook(int cpu, struct freq_update_hook *hook,
			void (*func)(struct freq_update_hook *hook,
				     enum sched_class_util sched_class,
				     u64 time, unsigned long util,
				     unsigned long max))
{
	if (WARN_ON(!hook || !func))
		return;

	hook->func = func;
	set_freq_update_hook(cpu, hook);
}
EXPORT_SYMBOL_GPL(cpufreq_set_freq_update_hook);

/**
 * cpufreq_set_update_util_hook - Clear the CPU's freq_update_hook pointer.
 * @cpu: The CPU to clear the pointer for.
 */
void cpufreq_clear_freq_update_hook(int cpu)
{
	set_freq_update_hook(cpu, NULL);
}
EXPORT_SYMBOL_GPL(cpufreq_clear_freq_update_hook);

/**
 * cpufreq_get_cfs_capacity_margin - Get global cfs enqueue capacity margin
 *
 * margin is a percentage of capacity that is applied to the current
 * utilization when selecting a new capacity state or cpu frequency. The value
 * should be normalized to the range of [0..SCHED_CAPACITY_SCALE], where
 * SCHED_CAPACITY_SCALE is 100% of the normalized capacity, or equivalent to
 * multiplying the utilization by one.
 *
 * This function returns the current global cfs enqueue capacity margin
 */
unsigned long cpufreq_get_cfs_capacity_margin(void)
{
	return cfs_capacity_margin;
}
EXPORT_SYMBOL_GPL(cpufreq_get_cfs_capacity_margin);

/**
 * cpufreq_set_cfs_capacity_margin - Set global cfs enqueue capacity margin
 * @margin: new capacity margin
 *
 * margin is a percentage of capacity that is applied to the current
 * utilization when selecting a new capacity state or cpu frequency. The value
 * should be normalized to the range of [0..SCHED_CAPACITY_SCALE], where
 * SCHED_CAPACITY_SCALE is 100% of the normalized capacity, or equivalent to
 * multiplying the utilization by one.
 *
 * For instance, to add a 25% margin to a utilization, margin should be 1280,
 * which is 1.25x 1024, the default for SCHED_CAPACITY_SCALE.
 */
void cpufreq_set_cfs_capacity_margin(unsigned long margin)
{
	cfs_capacity_margin = margin;
}
EXPORT_SYMBOL_GPL(cpufreq_set_cfs_capacity_margin);

/**
 * cpufreq_reset_cfs_capacity_margin - Reset global cfs enqueue cap margin
 *
 * margin is a percentage of capacity that is applied to the current
 * utilization when selecting a new capacity state or cpu frequency. The value
 * should be normalized to the range of [0..SCHED_CAPACITY_SCALE], where
 * SCHED_CAPACITY_SCALE is 100% of the normalized capacity, or equivalent to
 * multiplying the utilization by one.
 *
 * This function resets the global margin to its default value.
 */
void cpufreq_reset_cfs_capacity_margin(void)
{
	cfs_capacity_margin = CAPACITY_MARGIN_DEFAULT;
}
EXPORT_SYMBOL_GPL(cpufreq_reset_cfs_capacity_margin);

/**
 * cpufreq_update_util - Take a note about CPU utilization changes.
 * @time: Current time.
 * @util: CPU utilization.
 * @max: CPU capacity.
 *
 * This function is called on every invocation of update_load_avg() on the CPU
 * whose utilization is being updated.
 *
 * It can only be called from RCU-sched read-side critical sections.
 */
void cpufreq_update_util(enum sched_class_util sc, u64 time,
			 unsigned long util, unsigned long max)
{
	struct freq_update_hook *hook;

#ifdef CONFIG_LOCKDEP
	WARN_ON(debug_locks && !rcu_read_lock_sched_held());
#endif

	hook = rcu_dereference_sched(*this_cpu_ptr(&cpufreq_freq_update_hook));
	/*
	 * If this isn't inside of an RCU-sched read-side critical section, hook
	 * may become NULL after the check below.
	 */
	if (hook)
		hook->func(hook, sc, time, util, max);
}
