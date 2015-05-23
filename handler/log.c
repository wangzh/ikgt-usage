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
#include "utils.h"
#include "log.h"


/* hva to store logging data allocated by agent and passed to handler.
* It is for all CPUs and is equal to a log_entry_t array:
* g_log_data_hva[num_of_cpus][LOG_CPU_RECORD_NUM]
*/
static log_entry_t *g_log_data_hva;

/* VMEXIT log mask by VMEXIT reason: each VMEXIT reason uses a bit in the */
/* mask and set the bit means not recording it */
static uint64_t g_log_mask = 0x400;

static uint32_t g_log_size;
static uint64_t g_log_gva;

static void log_buffer_add_record(log_entry_t cpu_log_buffer_start[],
								  uint64_t rip, uint32_t reason, uint64_t qualification,
								  uint64_t gva);


/* Function Name: log_event
* Purpose: add event to buffer
*
* Input: IKGT Event Info
* Return value: none
*/
void log_event(ikgt_event_info_t *event_info)
{
	ikgt_vmexit_reason_t reason;
	log_entry_t *cpu_log_buffer; /* per cpu log buffer */
	uint64_t cpuid = event_info->thread_id;

	ikgt_get_vmexit_reason(&reason);
	if ((1L << reason.reason) & g_log_mask) {
		/* not recording the VMEXIT if the mask bit for the reason is set */
		return;
	}

	if (NULL == g_log_data_hva) {
		return;
	}

	cpu_log_buffer = get_cpu_log_buffer_start(g_log_data_hva, cpuid);

	log_buffer_add_record(cpu_log_buffer,
		event_info->vmcs_guest_state.ia32_reg_rip,
		reason.reason, reason.qualification,
		reason.gva);
}

/* Function Name: start_log
* Purpose: set log data storage addr to start profiling
*
* Input: IKGT Event Info
* Return value: none
*/
void start_log(ikgt_event_info_t *event_info, log_message_t *msg)
{
	ikgt_gva_to_gpa_params_t gva2gpa;
	ikgt_gpa_to_hva_params_t gpa2hva;
	ikgt_status_t status = IKGT_STATUS_SUCCESS;

	if (NULL == msg)
		return;

	DPRINTF("%s: log_addr=%llx, size=%u, view=%u\n",
		__func__, msg->log_addr, msg->log_size, event_info->view_handle);

	DPRINTF("ENTRIES_PER_CPU=%u\n", ENTRIES_PER_CPU);

	if (NULL == msg->log_addr) {
		return;
	}

	g_log_gva = (uint64_t)msg->log_addr;
	g_log_size = msg->log_size;

	/* translate the gva pages addr to hva */
	gva2gpa.guest_virtual_address = g_log_gva;
	gva2gpa.size = sizeof(ikgt_gva_to_gpa_params_t);
	gva2gpa.cr3 = 0;

	if (IKGT_STATUS_SUCCESS != ikgt_gva_to_gpa(
		&gva2gpa)) {
			return;
	}

	gpa2hva.view_handle = event_info->view_handle;
	gpa2hva.guest_physical_address = gva2gpa.guest_physical_address;
	if (IKGT_STATUS_SUCCESS != ikgt_gpa_to_hva(&gpa2hva)) {
		return;
	}

	g_log_data_hva = (void *)(gpa2hva.host_virtual_address);

	status = util_monitor_memory_ex(g_log_gva, g_log_size, PERMISSION_READ);
}

/* Function Name: stop_log
* Purpose: clear log data storage addr to stop profiling
*
* Input: IKGT Event Info
* Return value: none
*/
void stop_log(ikgt_event_info_t *event_info)
{
	ikgt_status_t status;

	if (NULL == g_log_data_hva)
		return;

	if (g_log_gva) {
		status = util_monitor_memory_ex(g_log_gva, g_log_size, PERMISSION_RWX);
	}

	g_log_data_hva = NULL;
}

static void log_buffer_add_record(log_entry_t cpu_log_buffer_start[],
								  uint64_t rip, uint32_t reason, uint64_t qualification,
								  uint64_t gva)
{
	log_entry_t *meta_entry;
	log_entry_t *data_entry;
	uint64_t next_seq_num;
	uint32_t index;

	meta_entry = &cpu_log_buffer_start[0];
	next_seq_num = meta_entry->meta.head;

	index = LOG_SEQ_NUM_TO_INDEX(next_seq_num);

	data_entry = &cpu_log_buffer_start[index];

	data_entry->data.rip = rip;
	data_entry->data.reason = reason;
	data_entry->data.qualification = qualification;
	data_entry->data.gva = gva;
	data_entry->data.seq_num = next_seq_num;
	data_entry->data.valid = 1;

	meta_entry->meta.head++;
}

#ifdef DEBUG
void log_debug_fill(void)
{
	int i;
	log_entry_t *cpu_log_buffer; /* per cpu log buffer */

	ikgt_printf("%s:\n", __func__);

	if (NULL == g_log_data_hva) {
		return;
	}

	cpu_log_buffer = get_cpu_log_buffer_start(g_log_data_hva, 3);

	for (i = 0; i < 10000; i++) {
		log_buffer_add_record(cpu_log_buffer,
			i,
			i + 3,
			i + 5,
			i + 7);
	}

	ikgt_printf("get_last_seq_num()=%llu\n", get_last_seq_num(cpu_log_buffer));
}

void log_debug(uint64_t command_code)
{
	ikgt_printf("%s(%u)\n", __func__, command_code);

	ikgt_printf("LOGS_PER_CPU=%u\n", LOGS_PER_CPU);

	/* log_debug_fill(); */
}
#endif

