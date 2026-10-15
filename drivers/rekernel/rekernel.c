/*
 * Re:Kernel netlink server (non-GKI integration)
 * Based on https://github.com/Sakion-Team/Re-Kernel (Integrate/README.md)
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/rekernel.h>

struct sock *rekernel_netlink;
static int netlink_unit = REKERNEL_NETLINK_MIN;
static struct proc_dir_entry *rekernel_dir, *rekernel_unit_entry;

int send_netlink_message(char *msg, uint16_t len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;

	if (!rekernel_ready())
		return -ENODEV;

	skb = nlmsg_new(len, GFP_ATOMIC);
	if (!skb) {
		pr_err("rekernel: netlink alloc failure\n");
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb, 0, 0, netlink_unit, len, 0);
	if (!nlh) {
		pr_err("rekernel: nlmsg_put failure\n");
		nlmsg_free(skb);
		return -EMSGSIZE;
	}

	memcpy(nlmsg_data(nlh), msg, len);
	return netlink_unicast(rekernel_netlink, skb, REKERNEL_USER_PORT,
			       MSG_DONTWAIT);
}

static void rekernel_rcv_msg(struct sk_buff *skb)
{
	/* userspace -> kernel messages are ignored */
}

static struct netlink_kernel_cfg rekernel_cfg = {
	.input = rekernel_rcv_msg,
};

static int rekernel_unit_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", netlink_unit);
	return 0;
}

static int rekernel_unit_open(struct inode *inode, struct file *file)
{
	return single_open(file, rekernel_unit_show, NULL);
}

static const struct file_operations rekernel_unit_fops = {
	.open		= rekernel_unit_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

static int __init rekernel_init(void)
{
	char name[16];
	struct sock *sk = NULL;

	for (netlink_unit = REKERNEL_NETLINK_MIN;
	     netlink_unit < REKERNEL_NETLINK_MAX; netlink_unit++) {
		sk = netlink_kernel_create(&init_net, netlink_unit,
					   &rekernel_cfg);
		if (sk)
			break;
	}
	if (!sk) {
		pr_err("rekernel: failed to create netlink server\n");
		return -ENODEV;
	}
	pr_info("rekernel: created server, NETLINK UNIT: %d\n", netlink_unit);

	rekernel_dir = proc_mkdir("rekernel", NULL);
	if (!rekernel_dir) {
		pr_err("rekernel: create /proc/rekernel failed\n");
	} else {
		snprintf(name, sizeof(name), "%d", netlink_unit);
		rekernel_unit_entry = proc_create(name, 0644, rekernel_dir,
						  &rekernel_unit_fops);
		if (!rekernel_unit_entry)
			pr_err("rekernel: create unit entry failed\n");
	}

	/* publish last: hooks start reporting only once everything exists */
	smp_store_release(&rekernel_netlink, sk);
	return 0;
}
device_initcall(rekernel_init);

MODULE_DESCRIPTION("Re:Kernel binder/signal reporting for frozen tasks");
MODULE_LICENSE("GPL");
