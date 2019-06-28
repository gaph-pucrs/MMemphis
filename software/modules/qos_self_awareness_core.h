/*
 * qos_self_awareness_core.h
 *
 *  Created on: Apr 20, 2017
 *      Author: ruaro
 */

#ifndef QOS_SELF_AWARENESS_CORE_H_
#define QOS_SELF_AWARENESS_CORE_H_

#include "packet.h"

typedef struct {
	int proc;
	int utilization;
	int computation_task_number;
	int num_RT_tasks;
} RemoteCPUData;


void QoS_analysis(unsigned int, ServiceHeader *);

void check_app_routine_overhaul();

inline int is_QoS_analysis_active();

void remote_CPU_data_protocol(int, int, int, int);

void handle_learned_profile_packet(int, int);

#endif /* QOS_SELF_AWARENESS_CORE_H_ */
