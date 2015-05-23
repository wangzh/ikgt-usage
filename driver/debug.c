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
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/kobject.h>
#include <linux/sysctl.h>
#include <linux/pagemap.h>

#include "ikgt_api.h"
#include "policy_common.h"
#include "common.h"
#include "debug.h"
#include "log.h"


#ifdef DEBUG
static int ikgt_agent_debug;

static int ikgt_systl_debug(struct ctl_table *ctl, int write,
							void __user *buffer, size_t *count,
							loff_t *ppos)
{
	unsigned long val;
	int len, rc;
	char buf[32];

	if (!*count || (*ppos && !write)) {
		*count = 0;
		return 0;
	}

	if (!write) {
		len = snprintf(buf, sizeof(buf), "%d\n", ikgt_agent_debug);
		rc = copy_to_user(buffer, buf, sizeof(buf));
		if (rc != 0)
			return -EFAULT;
	} else {
		len = *count;
		rc = kstrtoul_from_user(buffer, len, 0, &val);
		if (rc)
			return rc;

		ikgt_agent_debug = val;

		switch (val) {
		case 1000:
			test_log();
			break;

		default:
			ikgt_debug(val);
			break;
		}
	}
	*count = len;
	*ppos += len;


	return 0;
}

static struct ctl_table ikgt_sysctl_table[] = {
	{
		.procname	= "ikgt_agent_debug",
			.mode	= 0644,
			.proc_handler	= ikgt_systl_debug,
	},
	{}
};

static struct ctl_table kern_dir_table[] = {
	{
		.procname	= "kernel",
			.maxlen		= 0,
			.mode		= 0555,
			.child		= ikgt_sysctl_table,
	},
	{}
};

static struct ctl_table_header *ikgt_sysctl_header;


void ikgt_debug(uint64_t parameter)
{
	policy_message_t msg;
	ikgt_result_t ret;

	PRINTK_INFO("%s: parameter=%llu\n", __func__, parameter);

	msg.command =  POLICY_DEBUG;
	msg.count = 1;
	msg.debug_param.parameter = parameter;

	ret = ikgt_hypercall(IKGT_POLICY_MSG, (char *)&msg, NULL);
}

void init_debug(void)
{
	ikgt_sysctl_header = register_sysctl_table(kern_dir_table);
}

void uninit_debug(void)
{
	unregister_sysctl_table(ikgt_sysctl_header);
}
#endif
