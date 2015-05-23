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

#include "common.h"
#include "policy_common.h"
#include "log.h"


/* last logging data sequence number per CPU */
static uint64_t *log_record_seq_num;

/* variable used to form one log record as string for temporary */
#define MAX_LOG_RECORD_LEN 256

static uint32_t num_of_cpus;

/* if logging is running, log_data_gva must be not NULL and NMI handler */
/* must be registered and vise verse */
static bool is_logging_running;
static log_entry_t *log_data_gva;

#define MAX_SENTINEL_SIZE  64
#define MAX_ELLIPSIS_SIZE  4
#define MAX_CONFIGFS_PAGE_SIZE  (PAGE_4KB - MAX_SENTINEL_SIZE - MAX_ELLIPSIS_SIZE - 1)


static struct configfs_attribute log_children_attr_description = {
	.ca_owner	= THIS_MODULE,
	.ca_name	= "log.txt",
	.ca_mode	= S_IRUGO,
};

static struct configfs_attribute *log_children_attrs[] = {
	&log_children_attr_description,
	NULL,
};

static int dump_log(char *configfs_page);
static ssize_t log_children_attr_show(struct config_item *item,
struct configfs_attribute *attr,
	char *page)
{
	return dump_log(page);
}

static void log_children_release(struct config_item *item)
{
	kfree(to_node(item));
}

static struct configfs_item_operations log_children_item_ops = {
	.release	= log_children_release,
	.show_attribute = log_children_attr_show,
};

static struct config_item_type log_children_type = {
	.ct_item_ops	= &log_children_item_ops,
	.ct_attrs	= log_children_attrs,
	.ct_owner	= THIS_MODULE,
};

struct config_item_type *get_log_children_type(void)
{
	return &log_children_type;
}

/*Return: 1=configfs_page is full, 0=configfs_page is not full*/
static int log_add_msg_to_configfs(char *configfs_page, char *msg,
								   int msglen, int *offset)
{
	int copy;

	BUG_ON((*offset) >= (MAX_CONFIGFS_PAGE_SIZE - 1));

	copy = min(msglen, MAX_CONFIGFS_PAGE_SIZE - 1 - *offset);

	strncpy(configfs_page + *offset, msg, copy);
	*offset += copy;

	if (copy != msglen) {
		int n;

		n = min(4, MAX_ELLIPSIS_SIZE);
		strncpy(configfs_page + *offset, "...\n", n);
		*offset += n;
		copy += n;

		return 1;
	}

	if (*offset >= (MAX_CONFIGFS_PAGE_SIZE - 1))
		return 1;

	return 0;
}

/*
*   IN cpu_log_buffer: start of the per cpu log buffer
*   OUTPUT results: event contents copy to
*   IN start_seq_num: the sequence number to start report.
*   IN count: the number of log entries counting backwards
*   RETURN: number of logs actually read
*   Note: begin of log to be copied is from the given "start_seq_num" location
*         trace backward to number of "count".
*/
uint32_t read_logs(log_entry_t *cpu_log_buffer, log_entry_t results[],
				   uint64_t start_seq_num, int count)
{
	log_entry_t *entry;
	uint32_t index;
	uint32_t num_of_entries_copied = 0;

	if ((-1 == count) || (count > LOGS_PER_CPU))
		count = LOGS_PER_CPU;

	/* index = 1~LOGS_PER_CPU */
	index = LOG_SEQ_NUM_TO_INDEX(start_seq_num);

	if (index <= count)
		index = LOGS_PER_CPU - (count - index);
	else {
		index = index - count;
	}

	while (count) {
		entry = &cpu_log_buffer[index];

		if (entry->data.valid) {
			results[num_of_entries_copied++] = *entry;
		}

		if (index >= LOGS_PER_CPU) {
			index = 1;
		} else {
			index++;
		}

		if (index == LOG_SEQ_NUM_TO_INDEX(start_seq_num))
			break;

		count--;
	}

	return num_of_entries_copied;
}

/* cpu_log_buffer_start pointers to the beginning of the per cpu
*  log buffer. cpu_log_buffer_start is multiplexed:
*  first entry (index=0) stores meta data, entries 1 to (ENTRIES_PER_CPU - 1)
* stores the actual event logs
*/
static void log_buffer_init(log_entry_t cpu_log_buffer_start[])
{
	log_entry_t *meta_entry;

	meta_entry = &cpu_log_buffer_start[0];

	/* initialize next sequence number to 0 */
	meta_entry->meta.head = 0;
#ifdef DEBUG
	meta_entry->meta.test = 0;
#endif
}

static int dump_log(char *configfs_page)
{
	uint32_t cpu_index = 0;
	uint32_t log_index = 0;
	log_entry_t *cpu_log_buffer;
	int offset = 0;
	int n, full = 0;
	char *sz_log_record;
	uint64_t last_seq_num;
	uint32_t num_of_logs_returned;
	uint32_t num_of_logs_dumped = 0;
	uint32_t num_of_logs_to_read;
	log_entry_t *results = NULL;
	log_entry_t *entry;

	if (!configfs_page)
		return 0;

	if (NULL == log_record_seq_num)
		return 0;

	sz_log_record = (char *)kzalloc(MAX_LOG_RECORD_LEN + 1, GFP_KERNEL);
	if (NULL == sz_log_record)
		return 0;

	results = (log_entry_t *)kzalloc(LOGS_PER_CPU * sizeof(log_entry_t), GFP_KERNEL);
	if (NULL == results) {
		kfree(sz_log_record);
		return 0;
	}

	for (cpu_index = 0; cpu_index < num_of_cpus; cpu_index++) {

		cpu_log_buffer = get_cpu_log_buffer_start(log_data_gva, cpu_index);

		last_seq_num = get_last_seq_num(cpu_log_buffer);

		num_of_logs_to_read = last_seq_num - log_record_seq_num[cpu_index];

		log_record_seq_num[cpu_index] = last_seq_num;

		if ((0 == num_of_logs_to_read) || (0 == last_seq_num))
			continue;

		num_of_logs_returned = read_logs(cpu_log_buffer, results, last_seq_num, num_of_logs_to_read);

		for (log_index = 0; log_index < num_of_logs_returned; log_index++) {

			entry = &results[log_index];

			n = snprintf(sz_log_record, MAX_LOG_RECORD_LEN,
				"%u,%llu,%u,%llX,%llX,%llX\n",
				cpu_index, entry->data.seq_num, entry->data.reason,
				entry->data.qualification, entry->data.rip,
				entry->data.gva);

			full = log_add_msg_to_configfs(configfs_page, sz_log_record, n, &offset);
			if (full) {
				log_record_seq_num[cpu_index] = entry->data.seq_num;
				break;
			}
			num_of_logs_dumped++;
		}
		if (full)
			break;
	}

	n = snprintf(sz_log_record, MAX_SENTINEL_SIZE - 1, "%u,%u,%d\nEOF\n", offset, num_of_logs_dumped, full);
	strncpy(configfs_page + offset, sz_log_record, n);
	offset += n;

	kfree(sz_log_record);
	kfree(results);

	return offset;
}

char *init_log(uint32_t *log_size)
{
	uint32_t cpu_index = 0;
	uint32_t size;
	log_entry_t *log_buffer;

	num_of_cpus = num_online_cpus();

	log_record_seq_num =  kzalloc(num_of_cpus * sizeof(uint64_t), GFP_KERNEL);
	if (NULL == log_record_seq_num)
		return NULL;

	size = num_of_cpus * LOG_PAGES_PER_CPU * PAGE_4KB;

	if (log_size) {
		*log_size = size;
	}

	/* allocate memory for log data filled in by handler */
	log_data_gva = kzalloc(size, GFP_KERNEL);
	if (log_data_gva == NULL) {
		PRINTK_ERROR("failed to allocate memory for log data pages\n");
		kfree(log_record_seq_num);
		return NULL;
	}

	PRINTK_INFO("malloc log data pages at gva %#llx, size=%u\n",
		(uint64_t)log_data_gva, size);

	/* initialize all cpu circular buffers to empty */
	for (cpu_index = 0; cpu_index < num_of_cpus; ++cpu_index) {
		log_buffer = get_cpu_log_buffer_start(log_data_gva, cpu_index);
		log_buffer_init(log_buffer);
	}

	is_logging_running = true;

	return (char *)log_data_gva;
}

#ifdef DEBUG
void test_log(void)
{
	log_entry_t *cpu_log_buffer;

	cpu_log_buffer = get_cpu_log_buffer_start(log_data_gva, 0);

	PRINTK_INFO("Before: test=%llx\n",	cpu_log_buffer[0].meta.test);

	cpu_log_buffer[0].meta.test = 0x1698;

	PRINTK_INFO("After: test=%llx\n",	cpu_log_buffer[0].meta.test);
}
#endif

