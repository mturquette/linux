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
 * to call from cpufreq_trigger_update().  That function will be called from
 * an RCU read-side critical section, so it must not sleep.
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
			void (*func)(struct freq_update_hook *hook, u64 time))
{
	if (WARN_ON(!hook || !func))
		return;

	hook->func = func;
	set_freq_update_hook(cpu, hook);
}
EXPORT_SYMBOL_GPL(cpufreq_set_freq_update_hook);

/**
 * cpufreq_set_update_util_hook - Set the CPU's utilization update callback.
 * @cpu: The CPU to set the callback for.
 * @hook: New freq_update_hook pointer value.
 * @update_util: Callback function to use with the new hook.
 */
void cpufreq_set_update_util_hook(int cpu, struct freq_update_hook *hook,
		void (*update_util)(struct freq_update_hook *hook, u64 time,
				    unsigned long util, unsigned long max))
{
	if (WARN_ON(!hook || !update_util))
		return;

	hook->update_util = update_util;
	set_freq_update_hook(cpu, hook);
}
EXPORT_SYMBOL_GPL(cpufreq_set_update_util_hook);

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
void cpufreq_update_util(u64 time, unsigned long util, unsigned long max)
{
	struct freq_update_hook *hook;

#ifdef CONFIG_LOCKDEP
	WARN_ON(debug_locks && !rcu_read_lock_sched_held());
#endif

	hook = rcu_dereference(*this_cpu_ptr(&cpufreq_freq_update_hook));
	/*
	 * If this isn't inside of an RCU-sched read-side critical section, hook
	 * may become NULL after the check below.
	 */
	if (hook) {
		if (hook->update_util)
			hook->update_util(hook, time, util, max);
		else
			hook->func(hook, time);
	}
}

/**
 * cpufreq_trigger_update - Trigger CPU performance state evaluation if needed.
 * @time: Current time.
 *
 * The way cpufreq is currently arranged requires it to evaluate the CPU
 * performance state (frequency/voltage) on a regular basis.  To facilitate
 * that, cpufreq_update_util() is called by update_load_avg() in CFS when
 * executed for the current CPU's runqueue.
 *
 * However, this isn't sufficient to prevent the CPU from being stuck in a
 * completely inadequate performance level for too long, because the calls
 * from CFS will not be made if RT or deadline tasks are active all the time
 * (or there are RT and DL tasks only).
 *
 * As a workaround for that issue, this function is called by the RT and DL
 * sched classes to trigger extra cpufreq updates to prevent it from stalling,
 * but that really is a band-aid.  Going forward it should be replaced with
 * solutions targeted more specifically at RT and DL tasks.
 */
void cpufreq_trigger_update(u64 time)
{
	cpufreq_update_util(time, ULONG_MAX, 0);
}
