/*
 * Re:Kernel - report frozen-task activity (binder / signals) to userspace
 * https://github.com/Sakion-Team/Re-Kernel
 *
 * Wire protocol (NETLINK unit 22..25, user port 100, 128 byte packets) is
 * unchanged from upstream so existing userspace (Re:Kernel / NoActive /
 * MIUI-style freezers) keeps working.
 */
#ifndef _LINUX_REKERNEL_H
#define _LINUX_REKERNEL_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/string.h>

#define REKERNEL_NETLINK_MIN	22
#define REKERNEL_NETLINK_MAX	26	/* exclusive */
#define REKERNEL_USER_PORT	100
#define PACKET_SIZE		128
#define MIN_USERAPP_UID		(10000)
#define MAX_SYSTEM_UID		(2000)
#define RESERVE_ORDER		17
#define WARN_AHEAD_SPACE	(1 << RESERVE_ORDER)

#ifdef CONFIG_REKERNEL
extern struct sock *rekernel_netlink;
int send_netlink_message(char *msg, uint16_t len);

/* The netlink socket is created once at boot (device_initcall), so hooks
 * never have to sleep or allocate sockets from atomic context. */
static inline bool rekernel_ready(void)
{
	return READ_ONCE(rekernel_netlink) != NULL;
}
#else
static inline bool rekernel_ready(void) { return false; }
static inline int send_netlink_message(char *msg, uint16_t len) { return -1; }
#endif

static inline bool line_is_frozen(struct task_struct *task)
{
	return frozen(task->group_leader) || freezing(task->group_leader);
}

#endif /* _LINUX_REKERNEL_H */
