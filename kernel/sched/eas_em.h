/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mainline-style Energy Aware Scheduling wake-up placement, backported to
 * 4.9 from android-mainline (kernel/sched/fair.c: find_energy_efficient_cpu(),
 * compute_energy(), cpu_util_next()).
 *
 * #included from fair.c (after task_util_est() and before
 * select_task_rq_fair()) because it uses fair.c-static helpers.
 *
 * Structure follows mainline v5.4:
 *  - performance domains live on the root domain (rd->pd, built in
 *    core.c:build_perf_domains() from the Energy Model);
 *  - the tipping point is rd->overutilized;
 *  - energy is estimated from schedutil's view of utilization
 *    (schedutil_cpu_util(): ENERGY_UTIL for the work done, FREQUENCY_UTIL,
 *    which includes uclamp of the rq and of the candidate task, for the
 *    frequency the domain would run at);
 *  - candidate CPUs are filtered with the uclamp-aware utilization.
 *
 * Runtime switch: /proc/sys/kernel/sched_eas_em (default 1). EMS keeps
 * handling everything EAS declines (-1): over-utilized, no EM/schedutil, etc.
 */
#ifdef CONFIG_SCHED_EAS_EM

unsigned int sysctl_sched_eas_em = 1;

/*
 * Perf domains are rebuilt from process context: they need the hotplug and
 * sched-domain locks, and callers (cpufreq probe / governor start) may not
 * be allowed to take them.
 */
static void eas_em_rebuild_workfn(struct work_struct *work)
{
	rebuild_sched_domains_energy();
}
static DECLARE_WORK(eas_em_rebuild_work, eas_em_rebuild_workfn);

void sched_eas_em_rebuild(void)
{
	schedule_work(&eas_em_rebuild_work);
}

/* A task fits when it uses less than ~80% of the CPU capacity. */
#define eas_em_fits_capacity(util, cap)	((cap) * 1024 > (util) * 1280)

/*
 * Utilization of @cpu if @p were (or stayed) enqueued on @dst_cpu.
 */
static unsigned long eas_em_cpu_util_next(int cpu, struct task_struct *p,
					   int dst_cpu)
{
	unsigned long util = cpu_util(cpu);

	if (task_cpu(p) == cpu && dst_cpu != cpu)
		util -= min(util, task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	return min(util, capacity_orig_of(cpu));
}

/*
 * Estimate the energy of perf domain @pd if @p ran on @dst_cpu
 * (@dst_cpu == -1: energy without @p).
 *
 * The domain frequency is set by its most utilized CPU, as schedutil would
 * (FREQUENCY_UTIL: freqvar boost + uclamp, @p's clamp counted on @dst_cpu
 * only). The energy then scales with the actual work of each CPU
 * (ENERGY_UTIL).
 */
static unsigned long eas_em_compute_energy(struct task_struct *p, int dst_cpu,
					   struct perf_domain *pd)
{
	const struct cpumask *pd_mask = perf_domain_span(pd);
	unsigned long cpu_cap = capacity_orig_of(cpumask_first(pd_mask));
	unsigned long max_util = 0, sum_util = 0;
	int cpu;

	for_each_cpu_and(cpu, pd_mask, cpu_online_mask) {
		unsigned long cpu_util, util_cfs;
		struct task_struct *tsk = (cpu == dst_cpu) ? p : NULL;

		util_cfs = eas_em_cpu_util_next(cpu, p, dst_cpu);

		sum_util += schedutil_cpu_util(cpu, util_cfs, cpu_cap,
					       ENERGY_UTIL, NULL);
		cpu_util = schedutil_cpu_util(cpu, util_cfs, cpu_cap,
					      FREQUENCY_UTIL, tsk);
		max_util = max(max_util, cpu_util);
	}

	return em_pd_energy(pd->em_pd, max_util, sum_util, cpu_cap);
}

/*
 * find_energy_efficient_cpu(): wake-up placement. Returns the most energy
 * efficient CPU for @p, @prev_cpu if no CPU saves a meaningful (>6%) amount
 * of energy, or -1 if EAS can't be used right now (no perf domains, system
 * over-utilized) and the caller must fall back to the regular path.
 */
static int find_energy_efficient_cpu(struct task_struct *p, int prev_cpu)
{
	unsigned long prev_delta = ULONG_MAX, best_delta = ULONG_MAX;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	unsigned long base_energy = 0;
	int cpu, best_energy_cpu = prev_cpu;
	struct sched_domain *sd;
	struct perf_domain *pd;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd || READ_ONCE(rd->overutilized))
		goto fail;

	/*
	 * Energy-aware wake-up happens on the asymmetric CPU capacity domain
	 * that contains prev_cpu.
	 */
	sd = rcu_dereference(*this_cpu_ptr(&sd_asym_cpucapacity));
	while (sd && !cpumask_test_cpu(prev_cpu, sched_domain_span(sd)))
		sd = sd->parent;
	if (!sd)
		goto fail;

	if (!task_util_est(p))
		goto unlock;

	for (; pd; pd = pd->next) {
		unsigned long cur_delta, spare_cap, max_spare_cap = 0;
		unsigned long base_energy_pd;
		int max_spare_cap_cpu = -1;

		/* Compute the 'base' energy of the pd, without @p */
		base_energy_pd = eas_em_compute_energy(p, -1, pd);
		base_energy += base_energy_pd;

		for_each_cpu_and(cpu, perf_domain_span(pd), sched_domain_span(sd)) {
			unsigned long util, cpu_cap;

			if (!cpumask_test_cpu(cpu, tsk_cpus_allowed(p)) ||
			    !cpu_active(cpu))
				continue;

			util = eas_em_cpu_util_next(cpu, p, cpu);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap - min(cpu_cap, util);

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with schedutil_cpu_util().
			 */
			util = uclamp_rq_util_with(cpu_rq(cpu), util, p);
			if (!eas_em_fits_capacity(util, cpu_cap))
				continue;

			/* Always use prev_cpu as a candidate. */
			if (cpu == prev_cpu) {
				prev_delta = eas_em_compute_energy(p, prev_cpu, pd);
				prev_delta -= base_energy_pd;
				best_delta = min(best_delta, prev_delta);
			}

			/*
			 * Find the CPU with the maximum spare capacity in the
			 * domain: placing @p there minimizes the frequency
			 * increase for the domain.
			 */
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}
		}

		/* Evaluate the energy impact of using this CPU. */
		if (max_spare_cap_cpu >= 0 && max_spare_cap_cpu != prev_cpu) {
			cur_delta = eas_em_compute_energy(p, max_spare_cap_cpu, pd);
			cur_delta -= base_energy_pd;
			if (cur_delta < best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}
	}
unlock:
	rcu_read_unlock();

	/*
	 * Pick the best CPU if prev_cpu cannot be used, or if it saves at
	 * least 6% of the total energy used by the system.
	 */
	if (prev_delta == ULONG_MAX)
		return best_energy_cpu;

	if ((prev_delta - best_delta) > ((prev_delta + base_energy) >> 4))
		return best_energy_cpu;

	return prev_cpu;

fail:
	rcu_read_unlock();
	return -1;
}

#else /* !CONFIG_SCHED_EAS_EM */
static inline int find_energy_efficient_cpu(struct task_struct *p, int prev_cpu)
{
	return -1;
}
#endif /* CONFIG_SCHED_EAS_EM */
