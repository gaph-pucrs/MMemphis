/*
 * qos_self_adaptation.h
 *
 *  Created on: Apr 21, 2017
 *      Author: ruaro
 */

#ifndef QOS_SELF_ADAPTATION_H_
#define QOS_SELF_ADAPTATION_H_

#include "applications.h"

enum adaptation_types {TaskMigration, CircuitSwitching};

typedef struct {
	Task * task;					//!<ID of the new task
	int type;		//!<Type of adaptation 1 - migration, 2 - CS, 0 - empty slot
	CS_path * ctp;	//!< If task migration stores the new processor, If CS stores the target_task
} AdaptationRequest;

#define	MAX_ADPT_REQUEST	MAX_CLUSTER_APP*MAX_TASKS_APP

int migrate_RT(Task *);

int migrate_BE(Task *);

void handle_NI_CS_status(int, int);

AdaptationRequest * get_next_adapt_request();

int add_adaptation_request(Task *, CS_path *, int);

void init_QoS_adaptation();

#endif /* QOS_SELF_ADAPTATION_H_ */
