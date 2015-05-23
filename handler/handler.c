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


static boolean_t g_b_init_status = FALSE;


/* Function Name: handler_initialize
* Purpose: Initialize module specific handlers callback functions
*
* Input: num of cpus
* Return value: TRUE=success, FALSE=failure
*/
boolean_t handler_initialize(uint16_t num_of_cpus)
{
	ikgt_printf("HANDLER: Initializing Handler. Num of CPUs = %d\n",
		num_of_cpus);

	g_b_init_status = policy_initialize();
}

/* Function name: handler_report_event
*
* Purpose:
*         Start/stop profiling according to message and log events
*
* Input: IKGT Event Info
* Return: response code in ikgt_event_info_t
*
*/
void handler_report_event(ikgt_event_info_t *event_info)
{
	/* log handler only profiling so allow all other actions by default */
	event_info->response = IKGT_EVENT_RESPONSE_ALLOW;

	if (!g_b_init_status)
		return;

	/* memory events need special handling for agent */
	switch (event_info->type) {
	case IKGT_EVENT_TYPE_MEM:
		handle_memory_event(event_info);
		break;

	case IKGT_EVENT_TYPE_CPU:
		handle_cpu_event(event_info);
		break;

	case IKGT_EVENT_TYPE_MSG:
		handle_msg_event(event_info);
		break;
	}
}

