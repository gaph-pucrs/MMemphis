/*!\file task_control.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 *  This module defines function relative to task control block (TCB)
 * \detailed
 * The TCB structure is defined, this structure stores information of the user's task
 * that are running into each slave processor.
 */

#ifndef TASK_CONTROL_H_
#define TASK_CONTROL_H_

#include "task_scheduler.h"

/**
 * \brief This structure stores information of the user's task
 * that are running into each slave processor.
 */
typedef struct {
    unsigned int reg[30];       	//!<30 registers (Vn,An,Tn,Sn,RA)
    unsigned int pc;            	//!<program counter
    unsigned int offset;        	//!<initial address of the task code in page
    int       	 id;            	//!<identifier
	unsigned int text_lenght;   	//!<Memory TEXT section lenght in bytes
    unsigned int data_lenght;		//!<Memory DATA section lenght in bytes
    unsigned int bss_lenght;		//!<Memory BSS section lenght in bytes
    unsigned int proc_to_migrate;	//!<Processor to migrate the task
    unsigned int master_address;	//!<Master address of the task
    unsigned int send_data_addr;	//!<Points to the data address of the producer tasks when it calls SendService
    unsigned int remove_ctp;		//!<Flag (1|0) to remove a given CTP
    unsigned int add_ctp;			//!<Flag (1|0) to add a given CTP
    unsigned int is_service_task;	//!<If 0 means a user tasks, otherwise means a service task

    //Learning informations
    unsigned int communication_time;//!<Stores the total time spend in communication
    unsigned int computation_time;  //!<Stores the total time spend using CPU
    unsigned int total_comp;
    unsigned int total_comm;
/*
    int learned_profile[10];		 //!<Each position represent 100 us, -1 = communicating, 0 - idle, 1 using CPU
*/
    Scheduling * scheduling_ptr;	//!<Scheduling structure used by task scheduler

} TCB;

typedef struct {
	int id;
	int master_address;
} PendingMigrationTask;

void init_TCBs();

void send_learned_profile(unsigned int);

TCB * search_free_TCB();

TCB * searchTCB(unsigned int);

int is_another_task_running(int);

TCB * get_tcb_index_ptr(unsigned int);

void add_pending_migration_task(int, int);

int remove_pending_migration_task(int);

TCB * get_receiving_service_task(int);

#endif /* TASK_CONTROL_BLOCK_H_ */
