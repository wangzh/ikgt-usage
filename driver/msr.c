/*
* This is an example ikgt usage driver.
* Copyright (c) 2015, Intel Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#include <linux/module.h>
#include "ikgt_api.h"
#include "common.h"


name_value_map msr_regs[] = {
	{ "EFER",         0xC0000080, RESOURCE_ID_MSR_EFER},
	{ "STAR",         0xC0000081, RESOURCE_ID_MSR_STAR},
	{ "LSTAR",        0xC0000082, RESOURCE_ID_MSR_LSTAR},
	{ "SYSENTER_CS",  0x174,      RESOURCE_ID_MSR_SYSENTER_CS},
	{ "SYSENTER_ESP", 0x175,      RESOURCE_ID_MSR_SYSENTER_ESP},
	{ "SYSENTER_EIP", 0x176,      RESOURCE_ID_MSR_SYSENTER_EIP},
	{ "SYSENTER_PAT", 0x277,      RESOURCE_ID_MSR_SYSENTER_PAT},

	/* Table terminator */
	{}
};

static int valid_msr_attr(const char *name)
{
	int i;

	for (i = 0; msr_regs[i].name; i++) {
		if (strcasecmp(msr_regs[i].name, name) == 0) {
			return i;
		}
	}

	return -1;
}


static ssize_t msr_cfg_store_enable(struct msr_cfg *msr_cfg,
									const char *page,
									size_t count);

static ssize_t msr_cfg_store_write(struct msr_cfg *msr_cfg,
								   const char *page,
								   size_t count);

static ssize_t msr_cfg_store_sticky_value(struct msr_cfg *msr_cfg,
										  const char *page,
										  size_t count);

/* to_msr_cfg() function */
IKGT_CONFIGFS_TO_CONTAINER(msr_cfg);

/* define attribute structure */
CONFIGFS_ATTR_STRUCT(msr_cfg);

/* item operations */
IKGT_UINT32_SHOW(msr_cfg, enable);
IKGT_UINT32_HEX_SHOW(msr_cfg, write);
IKGT_ULONG_HEX_SHOW(msr_cfg, sticky_value);

/* attributes */
IKGT_CONFIGFS_ATTR_RW(msr_cfg, enable);
IKGT_CONFIGFS_ATTR_RW(msr_cfg, write);
IKGT_CONFIGFS_ATTR_RW(msr_cfg, sticky_value);

static struct configfs_attribute *msr_cfg_attrs[] = {
	&msr_cfg_attr_enable.attr,
	&msr_cfg_attr_write.attr,
	&msr_cfg_attr_sticky_value.attr,
	NULL,
};

CONFIGFS_ATTR_OPS(msr_cfg);


static bool policy_set_msr(struct msr_cfg *msr_cfg, bool enable)
{
	policy_message_t *msg = NULL;
	policy_update_rec_t *entry = NULL;
	ikgt_result_t ret;
	uint32_t size;
	int idx = valid_msr_attr(msr_cfg->item.ci_name);

	if (idx < 0)
		return false;

	size = sizeof(policy_message_t);
	msg = (policy_message_t *) kzalloc(size, GFP_KERNEL);
	if (msg == NULL)
		return false;

	msg->command = enable?POLICY_ENTRY_ENABLE:POLICY_ENTRY_DISABLE;
	msg->count = 1;

	entry = &msg->policy_data[0];

	POLICY_SET_RESOURCE_ID(entry, msr_regs[idx].res_id);
	POLICY_SET_WRITE_ACTION(entry, msr_cfg->write);

	POLICY_SET_STICKY_VALUE(entry, msr_cfg->sticky_value);

	ret = ikgt_hypercall(IKGT_POLICY_MSG, (char *)msg, NULL);

	kfree(msg);

	return (ret == SUCCESS)?true:false;
}

static ssize_t msr_cfg_store_write(struct msr_cfg *msr_cfg,
								   const char *page,
								   size_t count)
{
	unsigned long value;

	if (msr_cfg->locked)
		return -EPERM;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	msr_cfg->write = value;

	return count;
}

static ssize_t msr_cfg_store_sticky_value(struct msr_cfg *msr_cfg,
										  const char *page,
										  size_t count)
{
	unsigned long value;

	if (msr_cfg->locked)
		return -EPERM;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	msr_cfg->sticky_value = value;

	return count;
}

static ssize_t msr_cfg_store_enable(struct msr_cfg *msr_cfg,
									const char *page,
									size_t count)
{
	unsigned long value;
	bool ret = false;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	if (msr_cfg->locked) {
		return -EPERM;
	}

	ret = policy_set_msr(msr_cfg, value);

	if (ret) {
		msr_cfg->enable = value;
	}

	if (ret && (msr_cfg->write & POLICY_ACT_STICKY))
		msr_cfg->locked = true;

	return count;
}


static void msr_cfg_release(struct config_item *item)
{
	kfree(to_msr_cfg(item));
}

static struct configfs_item_operations msr_cfg_ops = {
	.release		= msr_cfg_release,
	.show_attribute		= msr_cfg_attr_show,
	.store_attribute	= msr_cfg_attr_store,
};

static struct config_item_type msr_cfg_type = {
	.ct_item_ops	= &msr_cfg_ops,
	.ct_attrs	= msr_cfg_attrs,
	.ct_owner	= THIS_MODULE,
};


static struct config_item *msr_make_item(struct config_group *group,
										 const char *name)
{
	struct msr_cfg *msr_cfg;

	if (valid_msr_attr(name) == -1) {
		PRINTK_ERROR("Invalid MSR bit name\n");
		return NULL;
	}

	msr_cfg = kzalloc(sizeof(struct msr_cfg), GFP_KERNEL);
	if (!msr_cfg) {
		return ERR_PTR(-ENOMEM);
	}

	config_item_init_type_name(&msr_cfg->item, name,
		&msr_cfg_type);

	return &msr_cfg->item;
}

static struct configfs_attribute msr_children_attr_description = {
	.ca_owner	= THIS_MODULE,
	.ca_name	= "description",
	.ca_mode	= S_IRUGO,
};

static struct configfs_attribute *msr_children_attrs[] = {
	&msr_children_attr_description,
	NULL,
};

static ssize_t msr_children_attr_show(struct config_item *item,
struct configfs_attribute *attr,
	char *page)
{
	return sprintf(page,
		"MSR\n"
		"\n"
		"Used in protected mode to control operations .  \n"
		"items are readable and writable.\n");
}

static void msr_children_release(struct config_item *item)
{
	kfree(to_node(item));
}

static struct configfs_item_operations msr_children_item_ops = {
	.release	= msr_children_release,
	.show_attribute = msr_children_attr_show,
};

static struct configfs_group_operations msr_children_group_ops = {
	.make_item	= msr_make_item,
};

static struct config_item_type msr_children_type = {
	.ct_item_ops	= &msr_children_item_ops,
	.ct_group_ops	= &msr_children_group_ops,
	.ct_attrs	= msr_children_attrs,
	.ct_owner	= THIS_MODULE,
};

struct config_item_type *get_msr_children_type(void)
{
	return &msr_children_type;
}
