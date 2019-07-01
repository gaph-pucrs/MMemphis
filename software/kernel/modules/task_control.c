/*!\file task_control.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * This module implements function relative to task control block (TCB)
 * This module is used by the slave kernel
 */

#include "task_control.h"

#include "../../../include/kernel_pkg.h"
#include "../../include/plasma.h"
#include "../../include/services.h"
#include "utils.h"
#include "packet.h"


TCB tcbs[MAX_LOCAL_TASKS];			//!<Local task TCB array

PendingMigrationTask pending_migration_tasks[MAX_LOCAL_TASKS];


void send_learned_profile(unsigned int obser_interval){

	ServiceHeader *p;
	TCB * tcb_ptr;
	unsigned int comp, comm;

	for(int i=0; i<MAX_LOCAL_TASKS; i++){

		tcb_ptr = &tcbs[i];
		//If task is valid and task is real-time
		if(tcb_ptr->id != FREE && tcb_ptr->scheduling_ptr->period){

			p = get_service_header_slot();

			p->header = tcb_ptr->master_address;

			p->service = LEARNED_TASK_PROFILE;

			p->task_ID = tcb_ptr->id;

			//Learning the profile
			comp =  tcb_ptr->total_comp * 100 / obser_interval;
			comm =  tcb_ptr->total_comm * 100 / obser_interval;

			/*if (net_address == 0x202){
				puts("\nLearn "); puts(itoa(tcb_ptr->id)); puts(": cp[");
				puts(itoa(comp));puts("] cm["); puts(itoa(comm)); puts("]  : "); puts(itoa(obser_interval));
				puts("\n");
			}*/

			p->task_profile = (comp << 16 | comm);

			send_packet(p, 0, 0);

			tcb_ptr->total_comm = 0;
			tcb_ptr->total_comp = 0;
		}

	}
}

/**Initializes TCB array
 */
void init_TCBs(){


	for(int i=0; i<MAX_LOCAL_TASKS; i++) {
		tcbs[i].id = -1;
		tcbs[i].pc = 0;
		tcbs[i].offset = PAGE_SIZE * (i + 1);
		tcbs[i].proc_to_migrate = -1;
		tcbs[i].remove_ctp = 0;
		tcbs[i].add_ctp = 0;
		tcbs[i].is_service_task = 0;

		//Inicializes learning profile
		tcbs[i].communication_time = 0;
		tcbs[i].computation_time = 0;
		tcbs[i].total_comm = 0;
		tcbs[i].total_comp = 0;

		init_scheduling_ptr(&tcbs[i].scheduling_ptr, i);

		tcbs[i].scheduling_ptr->tcb_ptr = (unsigned int) &tcbs[i];

		clear_scheduling( tcbs[i].scheduling_ptr );


		pending_migration_tasks[i].id = -1;
		pending_migration_tasks[i].master_address = -1;
	}
}

/**Search from a tcb position with status equal to FREE
 * \return The TCB pointer or 0 in a ERROR situation
 */
TCB* search_free_TCB() {

    for(int i=0; i<MAX_LOCAL_TASKS; i++){
		if(tcbs[i].scheduling_ptr->status == FREE){
			return &tcbs[i];
		}
	}

    puts("ERROR - no FREE TCB\n");
    while(1);
    return 0;
}

/**Search by a TCB
 * \param task_id Task ID to be searched
 * \return TCB pointer
 */
TCB * searchTCB(unsigned int task_id) {

    int i;

    for(i=0; i<MAX_LOCAL_TASKS; i++)
    	if(tcbs[i].id == task_id)
    		return &tcbs[i];

    return 0;
}

/**Gets the TCB pointer from a index
 * \param i Index of TCB
 * \return The respective TCB pointer
 */
TCB * get_tcb_index_ptr(unsigned int i){
	return &(tcbs[i]);
}

/**Test if there is another task of the same application running in the same slave processor
 * \param app_id Appliation ID
 * \return 1 - if YES, 0 if NO
 */
int is_another_task_running(int app_id){

	for (int i = 0; i < MAX_LOCAL_TASKS; i++){
		if (tcbs[i].scheduling_ptr->status != FREE && (tcbs[i].id >> 8) == app_id){
			return 1;
		}
	}
	return 0;
}

/** Insert a new pending migration task
 * \param task_id ID of pending task
 * \param master_address Master XY address of the pending task
 */
void add_pending_migration_task(int task_id, int master_address){
	for (int i = 0; i < MAX_LOCAL_TASKS; i++){
		if (pending_migration_tasks[i].id == -1){
			pending_migration_tasks[i].id = task_id;
			pending_migration_tasks[i].master_address = master_address;
			return;
		}
	}

	puts("ERROR - no FREE PendingMigrationTask Slot\n");
	while(1);
}

/** Remove a pending migration task
 * \param task_id ID of pending task
 * \return the Master address XY of the pending task
 */
int remove_pending_migration_task(int task_id){
	for (int i = 0; i < MAX_LOCAL_TASKS; i++){
		if (pending_migration_tasks[i].id == task_id){
			pending_migration_tasks[i].id = -1;
			return pending_migration_tasks[i].master_address;
		}
	}

	return -1;
}


/**Search for the task that have the ID equal to the target task id of the SendService call
 * \param task_id ID of consuming/receiving task
 * \return the TCB pointer of the consuming/receiver task
 */
TCB * get_receiving_service_task(int task_id){

	for (int i = 0; i < MAX_LOCAL_TASKS; i++){
		if (tcbs[i].scheduling_ptr->status != FREE && tcbs[i].scheduling_ptr->waiting_msg && tcbs[i].id == task_id)
			return &tcbs[i];
	}

	puts("ERROR - TCB not found for service task\n");
	while(1);

	return 0;
}


