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

#include <linux/sched.h>

static DEFINE_PER_CPU(struct freq_update_hook *, cpufreq_freq_update_hook);

/**
 * cpufreq_set_freq_update_hook - Populate the CPU's freq_update_hook pointer.
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
void cpufreq_set_freq_update_hook(int cpu, struct freq_update_hook *hook)
{
	if (WARN_ON(hook && !hook->func))
		return;

	rcu_assign_pointer(per_cpu(cpufreq_freq_update_hook, cpu), hook);
}
EXPORT_SYMBOL_GPL(cpufreq_set_freq_update_hook);

/**
 * cpufreq_trigger_update - Trigger CPU performance state evaluation if needed.
 * @time: Current time.
 *
 * The way cpufreq is currently arranged requires it to evaluate the CPU
 * performance state (frequency/voltage) on a regular basis.  To facilitate
 * that, this function is called by update_load_avg() in CFS when executed for
 * the current CPU's runqueue.
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
		hook->func(hook, time);
}
