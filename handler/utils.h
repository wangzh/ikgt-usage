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

#ifndef _UTILS_H
#define _UTILS_H

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))
#endif


ikgt_status_t read_guest_reg(ikgt_vmcs_guest_state_reg_id_t reg_id,
							 uint64_t *value);

ikgt_status_t write_guest_reg(ikgt_vmcs_guest_state_reg_id_t reg_id, uint64_t value);

ikgt_status_t get_ikgt_vmcs_guest_reg_id(ikgt_cpu_reg_t event_reg_id,
										 ikgt_vmcs_guest_state_reg_id_t *vmcs_reg_id);

ikgt_status_t util_monitor_memory(uint64_t start_addr, uint32_t size,
									uint32_t permission);

ikgt_status_t util_monitor_memory_ex(uint64_t start_addr, uint32_t size,
									uint32_t permission);

ikgt_status_t util_monitor_cpu_events(uint64_t cpu_bitmap[],
									  uint64_t mask,
									  ikgt_cpu_reg_t reg,
									  boolean_t enable);

ikgt_status_t util_monitor_msr(uint32_t msr_id, boolean_t enable);

#endif /* _UTILS_H */
