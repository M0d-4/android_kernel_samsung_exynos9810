// SPDX-License-Identifier: GPL-2.0
/* BORE 7.0.0 (Masahito Suzuki), CFS 4.9 adaptation. */
#include <linux/sched/bore.h>
#include <linux/log2.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/sysctl.h>

unsigned int sysctl_sched_bore __read_mostly = 1;
unsigned int sysctl_sched_burst_penalty_offset __read_mostly = 24;
unsigned int sysctl_sched_burst_penalty_scale __read_mostly = 1536;
unsigned int sysctl_sched_burst_smoothness __read_mostly = 1;
unsigned int sysctl_sched_burst_inherit_type __read_mostly = 2;

#define BORE_MAX_PENALTY ((40U << 8) - 1)

static u16 bore_penalty(u64 ns)
{
	u32 log, diff;
	int leading;

	if (!ns)
		return 0;
	leading = fls64(ns);
	log = (leading << 8) | ((ns << (64 - leading)) >> 56);
	if (log <= sysctl_sched_burst_penalty_offset * 256)
		return 0;
	diff = log - sysctl_sched_burst_penalty_offset * 256;
	return min_t(u64, (u64)diff * sysctl_sched_burst_penalty_scale >> 10,
		     BORE_MAX_PENALTY);
}

void bore_fork(struct task_struct *p, struct task_struct *parent)
{
	p->bore.burst_time = 0;
	p->bore.curr_penalty = 0;
	p->bore.prev_penalty = sysctl_sched_burst_inherit_type ?
		parent->bore.penalty : 0;
	p->bore.penalty = p->bore.prev_penalty;
}

void bore_sleep(struct task_struct *p)
{
	struct bore_ctx *bc = &p->bore;
	unsigned int smooth = min(sysctl_sched_burst_smoothness, 16U);

	if (!sysctl_sched_bore)
		return;
	bc->prev_penalty = (bc->prev_penalty * ((1U << smooth) - 1) +
			    bc->curr_penalty) >> smooth;
	bc->curr_penalty = 0;
	bc->penalty = bc->prev_penalty;
	bc->burst_time = 0;
}

void bore_update(struct task_struct *p, u64 delta_exec)
{
	struct bore_ctx *bc = &p->bore;
	unsigned int smooth = min(sysctl_sched_burst_smoothness, 16U);

	if (!sysctl_sched_bore)
		return;
	bc->burst_time = min_t(u64, U64_MAX - delta_exec,
				   bc->burst_time) + delta_exec;
	bc->curr_penalty = bore_penalty(bc->burst_time);
	bc->penalty = max_t(u16, bc->curr_penalty,
				 (bc->prev_penalty * ((1U << smooth) - 1) +
				  bc->curr_penalty) >> smooth);
}

u64 bore_vruntime_delta(struct task_struct *p, u64 delta)
{
	unsigned int penalty = min_t(unsigned int, p->bore.penalty >> 8, 39);

	if (!sysctl_sched_bore || !penalty)
		return delta;
	return min_t(u64, delta * (u64)(40 + penalty) / 40, U64_MAX);
}

int sched_bore_update_handler(struct ctl_table *table, int write,
			      void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return proc_dointvec_minmax(table, write, buffer, lenp, ppos);
}

int sched_burst_inherit_type_update_handler(struct ctl_table *table,
					    int write, void __user *buffer,
					    size_t *lenp, loff_t *ppos)
{
	return proc_dointvec_minmax(table, write, buffer, lenp, ppos);
}

EXPORT_SYMBOL(bore_fork);
EXPORT_SYMBOL(bore_sleep);
EXPORT_SYMBOL(bore_update);
EXPORT_SYMBOL(bore_vruntime_delta);