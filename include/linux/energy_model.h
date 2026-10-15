/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Energy Model of performance domains (backport for 4.9 of the framework
 * merged in mainline v5.0, cpu-based API, as shipped in android-mainline).
 *
 * Differences vs. mainline:
 *  - em_pd_energy() takes the CPU capacity as an argument (4.9 has no
 *    arch-independent arch_scale_cpu_capacity(cpu) usable from a header).
 *  - frequency/power units are whatever the registrant uses; on Exynos
 *    9810 they come from the EMS energy table (kHz, abstract power).
 */
#ifndef _LINUX_ENERGY_MODEL_H
#define _LINUX_ENERGY_MODEL_H

#include <linux/cpumask.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
#include <linux/types.h>

#ifdef CONFIG_ENERGY_MODEL
/**
 * em_cap_state - Capacity state of a performance domain
 * @frequency:	The CPU frequency in KHz
 * @power:	The power consumed by 1 CPU at this level
 * @cost:	The cost coefficient associated with this level, used during
 *		energy calculation. Equal to: power * max_frequency / frequency
 */
struct em_cap_state {
	unsigned long frequency;
	unsigned long power;
	unsigned long cost;
};

/**
 * em_perf_domain - Performance domain
 * @table:		List of capacity states, in ascending order
 * @nr_cap_states:	Number of capacity states
 * @cpus:		Cpumask covering the CPUs of the domain
 */
struct em_perf_domain {
	struct em_cap_state *table;
	int nr_cap_states;
	unsigned long cpus[0];
};

#define EM_CPU_MAX_POWER 0xFFFF

struct em_data_callback {
	/**
	 * active_power() - Provide power at the next capacity state of a CPU
	 * @power	: Active power at the capacity state in mW (modified)
	 * @freq	: Frequency at the capacity state in kHz (modified)
	 * @cpu		: CPU for which we do this operation
	 *
	 * active_power() must find the lowest capacity state of 'cpu' above
	 * 'freq' and update 'freq' and 'power' to match that state.
	 */
	int (*active_power)(unsigned long *power, unsigned long *freq, int cpu);
};
#define EM_DATA_CB(_active_power_cb) { .active_power = &_active_power_cb }

struct em_perf_domain *em_cpu_get(int cpu);
int em_register_perf_domain(cpumask_t *span, unsigned int nr_states,
			    struct em_data_callback *cb);

/**
 * em_pd_energy() - Estimates the energy consumed by the CPUs of a perf. domain
 * @pd		: performance domain for which energy has to be estimated
 * @max_util	: highest utilization among CPUs of the domain
 * @sum_util	: sum of the utilization of all CPUs in the domain
 * @scale_cpu	: capacity of the CPUs of the domain (SCHED_CAPACITY_SCALE units)
 *
 * Return: the sum of the energy consumed by the CPUs of the domain assuming
 * a capacity state satisfying the max utilization of the domain.
 */
static inline unsigned long em_pd_energy(struct em_perf_domain *pd,
				unsigned long max_util, unsigned long sum_util,
				unsigned long scale_cpu)
{
	unsigned long freq, fmax;
	struct em_cap_state *cs;
	int i;

	if (!sum_util || !scale_cpu)
		return 0;

	/*
	 * In order to predict the capacity state, map the utilization of the
	 * most utilized CPU of the performance domain to a requested frequency,
	 * like schedutil: next_freq = 1.25 * max_freq * util / max_cap.
	 */
	cs = &pd->table[pd->nr_cap_states - 1];
	fmax = cs->frequency;
	freq = (fmax + (fmax >> 2)) * max_util / scale_cpu;

	/* Find the lowest capacity state of the domain that can serve freq. */
	for (i = 0; i < pd->nr_cap_states; i++) {
		cs = &pd->table[i];
		if (cs->frequency >= freq)
			break;
	}

	/*
	 * energy = sum_i(power_cs * util_i / cap_cs) with
	 * power_cs / cap_cs = cost_cs / scale_cpu  (see em_cap_state::cost)
	 */
	return cs->cost * sum_util / scale_cpu;
}

static inline int em_pd_nr_cap_states(struct em_perf_domain *pd)
{
	return pd->nr_cap_states;
}

#else /* CONFIG_ENERGY_MODEL */
struct em_perf_domain {};
struct em_data_callback {};
#define EM_DATA_CB(_active_power_cb) { }

static inline int em_register_perf_domain(cpumask_t *span,
			unsigned int nr_states, struct em_data_callback *cb)
{
	return -EINVAL;
}
static inline struct em_perf_domain *em_cpu_get(int cpu)
{
	return NULL;
}
static inline unsigned long em_pd_energy(struct em_perf_domain *pd,
			unsigned long max_util, unsigned long sum_util,
			unsigned long scale_cpu)
{
	return 0;
}
static inline int em_pd_nr_cap_states(struct em_perf_domain *pd)
{
	return 0;
}
#endif /* CONFIG_ENERGY_MODEL */

#endif /* _LINUX_ENERGY_MODEL_H */
