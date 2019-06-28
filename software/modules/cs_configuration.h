/*
 * cs_configuration.h
 *
 *  Created on: 06/10/2016
 *      Author: mruaro
 */

#ifndef CS_CONFIGURATION_H_
#define CS_CONFIGURATION_H_

#include "applications.h"

//Decision mode implemented inside qos_decision function
#define QOS_GENERATE_MODE			0
#define QOS_RELEASE_CS_MODE			1
#define QOS_STABLISH_CS_MODE		2
#define QOS_REQUEST_CS_MODE			3

//CS_Path Status
#define CS_NOT_AVAILABLE		-4
#define WAITING_CS_ACK			-3
#define CS_PENDING				-2
#define CS_NOT_ALLOCATED		-1
#define LOCAL_ALLOCATION		0

#define MAX_CTP_FOR_TASK			MAX_TASKS_APP*2

typedef struct {
	unsigned int 	ctp_id[MAX_CTP_FOR_TASK];
	CS_path * 		ctp_pointer[MAX_CTP_FOR_TASK];
	unsigned int 	ctp_size;
	int 			connections_in;
	int 			connections_out;
	unsigned int 	ack_counter;
	int 			task_id;
} ConnectionManager;

unsigned int sdn_setup_latency; //Usada so pra coletar overhead de SDN, apagar

inline int is_CS_NOT_active();

void request_runtime_CS(int, int, int, int);

void set_runtime_CS(int, int);

void concludes_runtime_CS(int, int);

void cancel_CS_reconfiguration();

void release_task_CS(Application *, int);

void release_CS_before_migration(int, int);

void init_CS_configuration();

void task_migration_notification(int);

int get_CTP_number(int, int);

void CS_Controller_NI_status_check(int);

#endif /* CS_CONFIGURATION_H_ */
