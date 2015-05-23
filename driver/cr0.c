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

static name_value_map cr0_bits[] = {
	{ "PE", PE, RESOURCE_ID_CR0_PE},
	{ "MP", MP, RESOURCE_ID_CR0_MP},
	{ "EM", EM, RESOURCE_ID_CR0_EM},
	{ "TS", TS, RESOURCE_ID_CR0_TS},
	{ "ET", ET, RESOURCE_ID_CR0_ET},
	{ "NE", NE, RESOURCE_ID_CR0_NE},
	{ "WP", WP, RESOURCE_ID_CR0_WP},
	{ "AM", AM, RESOURCE_ID_CR0_AM},
	{ "NW", NW, RESOURCE_ID_CR0_NW},
	{ "CD", CD, RESOURCE_ID_CR0_CD},
	{ "PG", PG, RESOURCE_ID_CR0_PG},

	/* Table terminator */
	{}
};

static ssize_t cr0_cfg_store_enable(struct cr0_cfg *cr0_cfg,
									const char *page,
									size_t count);

static ssize_t cr0_cfg_store_write(struct cr0_cfg *cr0_cfg,
								   const char *page,
								   size_t count);

static ssize_t cr0_cfg_store_sticky_value(struct cr0_cfg *cr0_cfg,
										  const char *page,
										  size_t count);

/* to_cr0_cfg() function */
IKGT_CONFIGFS_TO_CONTAINER(cr0_cfg);

/* define attribute structure */
CONFIGFS_ATTR_STRUCT(cr0_cfg);

/* item operations */
IKGT_UINT32_SHOW(cr0_cfg, enable);
IKGT_UINT32_HEX_SHOW(cr0_cfg, write);
IKGT_ULONG_HEX_SHOW(cr0_cfg, sticky_value);

/* attributes */
IKGT_CONFIGFS_ATTR_RW(cr0_cfg, enable);
IKGT_CONFIGFS_ATTR_RW(cr0_cfg, write);
IKGT_CONFIGFS_ATTR_RW(cr0_cfg, sticky_value);

static struct configfs_attribute *cr0_cfg_attrs[] = {
	&cr0_cfg_attr_enable.attr,
	&cr0_cfg_attr_write.attr,
	&cr0_cfg_attr_sticky_value.attr,
	NULL,
};

CONFIGFS_ATTR_OPS(cr0_cfg);


static int valid_cr0_attr(const char *name)
{
	int i;

	for (i = 0; cr0_bits[i].name; i++) {
		if (strcasecmp(cr0_bits[i].name, name) == 0) {
			return i;
		}
	}

	return -1;
}

/*-------------------------------------------------------*
*  Function      : policy_set_cr0()
*  Purpose: send the CR0 policy settings to handler
*  Parameters: cr0_cfg, enable
*  Return: true=success, false=failure
*-------------------------------------------------------*/
static bool policy_set_cr0(struct cr0_cfg *cr0_cfg, bool enable)
{
	policy_message_t *msg = NULL;
	policy_update_rec_t *entry = NULL;
	ikgt_result_t ret;
	uint32_t size;
	int idx = valid_cr0_attr(cr0_cfg->item.ci_name);

	if (idx < 0)
		return false;

	size = sizeof(policy_message_t);
	msg = (policy_message_t *) kzalloc(size, GFP_KERNEL);
	if (msg == NULL)
		return false;

	msg->command = enable?POLICY_ENTRY_ENABLE:POLICY_ENTRY_DISABLE;
	msg->count = 1;

	entry = &msg->policy_data[0];

	POLICY_SET_RESOURCE_ID(entry, cr0_bits[idx].res_id);
	POLICY_SET_WRITE_ACTION(entry, cr0_cfg->write);

	POLICY_SET_STICKY_VALUE(entry, cr0_cfg->sticky_value);

	POLICY_INFO_SET_MASK(entry, cr0_bits[idx].value);
	POLICY_INFO_SET_CPU_MASK_1(entry, -1);
	POLICY_INFO_SET_CPU_MASK_2(entry, -1);

	PRINTK_INFO("cpumask: %llx, %llx\n",
		POLICY_INFO_GET_CPU_MASK_1(entry), POLICY_INFO_GET_CPU_MASK_2(entry));

	ret = ikgt_hypercall(IKGT_POLICY_MSG, (char *)msg, NULL);

	kfree(msg);

	return (ret == SUCCESS)?true:false;
}

static ssize_t cr0_cfg_store_write(struct cr0_cfg *cr0_cfg,
								   const char *page,
								   size_t count)
{
	unsigned long value;

	if (cr0_cfg->locked)
		return -EPERM;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	cr0_cfg->write = value;

	return count;
}

static ssize_t cr0_cfg_store_sticky_value(struct cr0_cfg *cr0_cfg,
										  const char *page,
										  size_t count)
{
	unsigned long value;

	if (cr0_cfg->locked)
		return -EPERM;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	cr0_cfg->sticky_value = value;

	return count;
}

static ssize_t cr0_cfg_store_enable(struct cr0_cfg *cr0_cfg,
									const char *page,
									size_t count)
{
	unsigned long value;
	bool ret = false;

	if (kstrtoul(page, 0, &value))
		return -EINVAL;

	if (cr0_cfg->locked) {
		PRINTK_INFO("Sticky is set and locked!\n");
		return -EPERM;
	}

	ret = policy_set_cr0(cr0_cfg, value);

	if (ret) {
		cr0_cfg->enable = value;
	}

	if (ret && (cr0_cfg->write & POLICY_ACT_STICKY))
		cr0_cfg->locked = true;

	return count;
}

static void cr0_cfg_release(struct config_item *item)
{
	kfree(to_cr0_cfg(item));
}

static struct configfs_item_operations cr0_cfg_ops = {
	.release		= cr0_cfg_release,
	.show_attribute		= cr0_cfg_attr_show,
	.store_attribute	= cr0_cfg_attr_store,
};

static struct config_item_type cr0_cfg_type = {
	.ct_item_ops	= &cr0_cfg_ops,
	.ct_attrs	= cr0_cfg_attrs,
	.ct_owner	= THIS_MODULE,
};


static struct config_item *cr0_make_item(struct config_group *group,
										 const char *name)
{
	struct cr0_cfg *cr0_cfg;

	PRINTK_INFO("create attr name %s\n", name);

	if (valid_cr0_attr(name) == -1) {
		PRINTK_ERROR("Invalid CR0 bit name\n");
		return NULL;
	}

	cr0_cfg = kzalloc(sizeof(struct cr0_cfg), GFP_KERNEL);
	if (!cr0_cfg) {
		return ERR_PTR(-ENOMEM);
	}

	config_item_init_type_name(&cr0_cfg->item, name,
		&cr0_cfg_type);


	return &cr0_cfg->item;
}

static struct configfs_attribute cr0_children_attr_description = {
	.ca_owner	= THIS_MODULE,
	.ca_name	= "description",
	.ca_mode	= S_IRUGO,
};

static struct configfs_attribute *cr0_children_attrs[] = {
	&cr0_children_attr_description,
	NULL,
};

static ssize_t cr0_children_attr_show(struct config_item *item,
struct configfs_attribute *attr,
	char *page)
{
	return sprintf(page,
		"CR0\n"
		"\n"
		"Used in protected mode to control operations .  \n"
		"items are readable and writable.\n");
}

static void cr0_children_release(struct config_item *item)
{
	kfree(to_node(item));
}

static struct configfs_item_operations cr0_children_item_ops = {
	.release	= cr0_children_release,
	.show_attribute = cr0_children_attr_show,
};

static struct configfs_group_operations cr0_children_group_ops = {
	.make_item	= cr0_make_item,
};

static struct config_item_type cr0_children_type = {
	.ct_item_ops	= &cr0_children_item_ops,
	.ct_group_ops	= &cr0_children_group_ops,
	.ct_attrs	= cr0_children_attrs,
	.ct_owner	= THIS_MODULE,
};

struct config_item_type *get_cr0_children_type(void)
{
	return &cr0_children_type;
}
