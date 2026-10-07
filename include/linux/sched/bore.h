/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_BORE_H
#define _LINUX_SCHED_BORE_H

#include <linux/sched.h>

/* BORE 7.0.0 burst penalty adapted to CFS vruntime on Linux 4.9. */
extern unsigned int sysctl_sched_bore;
extern unsigned int sysctl_sched_burst_penalty_offset;
extern unsigned int sysctl_sched_burst_penalty_scale;
extern unsigned int sysctl_sched_burst_smoothness;
extern unsigned int sysctl_sched_burst_inherit_type;

void bore_fork(struct task_struct *p, struct task_struct *parent);
void bore_sleep(struct task_struct *p);
void bore_update(struct task_struct *p, u64 delta_exec);
u64 bore_vruntime_delta(struct task_struct *p, u64 delta);

#endif
