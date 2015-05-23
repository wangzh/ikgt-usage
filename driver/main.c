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
#include "configfs_setup.h"
#include "log.h"
#include "debug.h"


static int __init init_agent(void)
{
	policy_message_t msg;
	ikgt_result_t ret;

#ifdef DEBUG
	init_debug();
#endif

	PRINTK_INFO("%s:\n", __func__);

	msg.command = POLICY_INIT_LOG;
	msg.count = 1;
	msg.log_param.log_addr = init_log(&msg.log_param.log_size);
	if (NULL == msg.log_param.log_addr) {
		PRINTK_ERROR("failed to setup iKGT\n");
		return 1;
	}
	ret = ikgt_hypercall(IKGT_POLICY_MSG, (char *)&msg, NULL);
	if (SUCCESS != ret) {
		PRINTK_ERROR("failed to send message");
		return 1;
	}

	PRINTK_INFO("log_addr=%p\n", msg.log_param.log_addr);

	init_configfs_setup();

	return 0;
}

static void __exit exit_agent(void)
{
	exit_configfs_setup();

#ifdef DEBUG
	uninit_debug();
#endif
}


MODULE_LICENSE("GPL");
module_init(init_agent);
module_exit(exit_agent);
