/*******************************************************************************
* Copyright (c) 2015 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#include "handler.h"
#include "policy.h"
#include "log.h"
#include "utils.h"


void handle_msg_init(ikgt_event_info_t *event_info, log_message_t *msg)
{
	start_log(event_info, msg);
}

/* Function name: handle_msg_event
*
* Purpose:
*         Contains policy to handle agent msg events
*
* Input: IKGT Event Info
* Return: None
*
*/
void handle_msg_event(ikgt_event_info_t *event_info)
{
	policy_message_t *msg = NULL;
	ikgt_status_t status;

	msg = ikgt_malloc(sizeof(policy_message_t));
	if (NULL == msg) {
		return;
	}

	status = ikgt_copy_gva_to_hva((gva_t)event_info->event_specific_data,
		sizeof(policy_message_t), (hva_t)msg);

	if (IKGT_STATUS_SUCCESS != status) {
		ikgt_free(msg);
		return;
	}

	DPRINTF("%s: command=%d, count=%d\n", __func__, msg->command, msg->count);

	switch (msg->command) {
	case POLICY_INIT_LOG:
		handle_msg_init(event_info, &msg->log_param);
		break;

	case POLICY_ENTRY_ENABLE:
		handle_msg_policy_enable(event_info, &msg->policy_data[0]);
		break;

	case POLICY_ENTRY_DISABLE:
		handle_msg_policy_disable(event_info, &msg->policy_data[0]);
		break;

	case POLICY_MAKE_IMMUTABLE:
		handle_msg_policy_make_immutable(event_info, &msg->policy_data[0]);
		break;

#ifdef DEBUG
	case POLICY_DEBUG:
		handle_msg_debug(event_info, &msg->debug_param);
		break;
#endif
	}

	ikgt_free(msg);
}

#ifdef DEBUG
void handle_msg_debug(ikgt_event_info_t *event_info, debug_message_t *msg)
{
	memory_debug(msg->parameter);

	cpu_debug(msg->parameter);

	policy_debug(event_info, msg);

	log_debug(msg->parameter);
}
#endif


