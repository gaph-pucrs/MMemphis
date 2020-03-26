/*!\file task_location.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * This module defines function relative to task location structure.
 * \detailed
 * The TaskLocation structure is defined, this structure stores the location (slave process address) of the other task
 *
 */

#ifndef TASK_LOCATION_H_
#define TASK_LOCATION_H_

#include "../../../include/kernel_pkg.h"
#include "TCB.h"
#include "packet.h"

#define MAX_TASK_LOCATION	(MAX_LOCAL_TASKS*MAX_TASKS_APP)
#define MIGRATION_ENABLED			1		//!< Enable or disable the migration module
/**
 * \brief This structure stores the location (slave process address) of the other task
 */
typedef struct {
	int id;				//!<ID of task
	int proc_address;	//!<processor address of the task
} TaskLocation;

void init_task_location();

int get_task_location(int);

void add_task_location(int, int);

int remove_task_location(int);

void clear_app_tasks_locations(int);

void set_task_release(unsigned int, char);

void send_task_allocated(TCB *, int);

void send_task_terminated(TCB *);

void handle_task_allocation(volatile ServiceHeader *);


#endif /* TASK_LOCATION_H_ */
