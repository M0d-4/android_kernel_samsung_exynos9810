/*
 * Multi-Gen LRU framework for 4.9 (state, sysfs switch and static key).
 *
 * Reclaim is NOT redirected here: shrink_node() keeps using the stock
 * active/inactive LRU lists, so enabling this cannot change reclaim
 * results or mark memory as freed that was not freed.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/string.h>
#include <linux/sysfs.h>

DEFINE_STATIC_KEY_FALSE(lru_gen_caps);

static unsigned int min_ttl_ms;

void lru_gen_init_lruvec(struct lruvec *lruvec)
{
	struct lru_gen_struct *lrugen = &lruvec->lrugen;
	int gen;

	lrugen->max_seq = MIN_NR_GENS + 1;
	lrugen->min_seq[0] = MIN_NR_GENS;
	lrugen->min_seq[1] = MIN_NR_GENS;
	for (gen = 0; gen < MAX_NR_GENS; gen++)
		lrugen->timestamps[gen] = jiffies;
}

static ssize_t enabled_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "0x%04x\n", lru_gen_enabled() ? 1 : 0);
}

static ssize_t enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t len)
{
	bool enable;
	unsigned int val;

	/* accepts "y"/"n"/"1"/"0" as well as upstream style masks ("0x0007") */
	if (kstrtobool(buf, &enable)) {
		if (kstrtouint(buf, 0, &val))
			return -EINVAL;
		enable = !!val;
	}

	if (enable && !lru_gen_enabled())
		static_branch_enable(&lru_gen_caps);
	else if (!enable && lru_gen_enabled())
		static_branch_disable(&lru_gen_caps);

	return len;
}

static ssize_t min_ttl_ms_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_ttl_ms);
}

static ssize_t min_ttl_ms_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t len)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;
	min_ttl_ms = val;
	return len;
}

static struct kobj_attribute lru_gen_enabled_attr =
	__ATTR(enabled, 0644, enabled_show, enabled_store);
static struct kobj_attribute lru_gen_min_ttl_ms_attr =
	__ATTR(min_ttl_ms, 0644, min_ttl_ms_show, min_ttl_ms_store);

static struct attribute *lru_gen_attrs[] = {
	&lru_gen_enabled_attr.attr,
	&lru_gen_min_ttl_ms_attr.attr,
	NULL,
};

static const struct attribute_group lru_gen_attr_group = {
	.name = "lru_gen",
	.attrs = lru_gen_attrs,
};

static int __init lru_gen_init(void)
{
	int err;

#ifdef CONFIG_LRU_GEN_ENABLED
	static_branch_enable(&lru_gen_caps);
#endif
	err = sysfs_create_group(mm_kobj, &lru_gen_attr_group);
	if (err)
		pr_err("lru_gen: failed to create sysfs group (%d)\n", err);
	return 0;
}
subsys_initcall(lru_gen_init);
