/*
 * application.h
 *
 *  Created on: Jul 4, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_APPLICATION_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_APPLICATION_H_

#include "../common_include.h"
#include "globals.h"


//Application status
#define	 RUNNING				1	//!< Signals that the application have all its task mapped and the task already has been requested
#define	 FREE					2	//!< Signals that all task of the application finishes
#define	 WAITING_RECLUSTERING	3	//!< Signals that the application have at least one task waiting for reclustering
#define	 READY_TO_LOAD			4	//!< Signals that the application have all its task mapped but the task not were requested yet

//Task status
#define	 REQUESTED				0	//!< Signals that the task has already requested to the global master
#define	 ALLOCATED				1	//!< Signals that the task has successfully allocated into a processor
#define	 TASK_RUNNING			2	//!< Signals that the task is running on the processor
#define	 TERMINATED_TASK		3	//!< Signals that the task terminated its execution
#define	 MIGRATING				4	//!< Signals that the task was set to be migrated on the slave kernel

#define MAX_TASK_DEPENDECES		10	//!< Stores maximum number of dependences task that a task can have

/**
 * \brief This structure stores the communication dependences of a given task
 * \detailed This structure is fulfilled by the kernel master during the reception of a new application.
 * Thus, the application repository keeps the dependence information for each task
 */
typedef struct {
	int task;		//!< Stores the communicating task ID
	int flits;		//!< Stores number of flits exchanged between the source task and the communicating task
} Dependence;

/**
 * \brief This structure stores variables useful to manage a task from a manager kernel perspective
 * \detailed Some of the values are loaded from repository others, changed at runtime
 */
typedef struct {
	int  id;						//!< Stores the task id
	int  code_size;					//!< Stores the task code size - loaded from repository
	int  data_size;					//!< Stores the DATA memory section size - loaded from repository
	int  bss_size;					//!< Stores the BSS memory section size - loaded from repository
	int  allocated_proc;			//!< Stores the allocated processor address of the task
	int  computation_load;			//!< Stores the computation load
	int  borrowed_master;			//!< Stores the borrowed master address
	int  status;					//!< Stores the status
	int  dependences_number;		//!< Stores number of communicating task, i.e, dependences
	Dependence dependences[1];		//!< Stores task dependence array with a size equal to MAX_TASK_DEPENDECES
} Task;

/** \brief This structure store variables useful to the kernel master manage an application instance
 */
typedef struct {
	int app_ID;					//!< Stores the application id
	int status;					//!< Stores the application status
	int tasks_number;			//!< Stores the application task number
	int terminated_tasks;		//!< Stores the number of task terminated of an application
	Task tasks[MAX_TASKS_APP];	//!< Array of Task with the size equal to the MAX_TASKS_APP
} Application;


//Globals
Application applications[MAX_CLUSTER_TASKS];	//!< Store the applications informations, is equivalent to a attribute in OO paradigm


Application * get_application_ptr(int);

Application * get_next_pending_app();

int set_task_allocated(Application *, int);

void set_task_terminated(Application *, int);

void set_task_migrating(int);

void set_task_migrated(int, int);

unsigned int get_app_id_counter();

Application * read_and_create_application(unsigned int, unsigned int *);

void remove_application(int);

void initialize_applications();



/** Receives and app ID and return the Application pointer for the required app ID.
 * If not found, the kernel entering in a error situation
 *  \param app_id ID of the application
 *  \return Pointer for Application
 */
Application * get_application_ptr(int app_id){

	for(int i=0; i<MAX_CLUSTER_TASKS; i++){
		if (applications[i].app_ID == app_id){
			return &applications[i];
		}
	}

	Puts("ERROR: Applications not found "); Puts(itoa(app_id)); Puts("\n");
	while(1);
	return 0;
}

/** Searches for a Task pointer into an Application instance. If not found, the kernel entering in a error situation
 *  \param app Wanted Application pointer
 *  \return Task pointer
 */
Task * get_task_ptr(Application * app, int task_id){

	for(int i=0; i<app->tasks_number; i++){

		if (app->tasks[i].id == task_id){

			return &app->tasks[i];

		}
	}

	Puts("ERROR: Task not found "); Puts(itoa(task_id)); Puts("\n");
	while(1);
	return 0;
}


/** Get the oldest application waiting reclustering
 *  \return Selected Application pointer
 */
Application * get_next_pending_app(){

	Application * older_app = 0;

	for(int i=0; i<MAX_CLUSTER_TASKS; i++){
		if (applications[i].status == WAITING_RECLUSTERING){

			if (older_app == 0 || older_app->app_ID > applications[i].app_ID){
				older_app = &applications[i];
			}
		}
	}

	if (!older_app){
		Puts("ERROR: no one app waiting reclustering\n");
		while(1);
	}

	return older_app;
}

/** Set a task status as allocated and verifies the number of allocated task for the application
 * \param app Application pointer of the task
 * \param task_id ID of the allocated task
 * \return The number of allocated task for the application
 */
int set_task_allocated(Application * app, int task_id){

	int task_allocated = 0;

	for(int i=0; i<app->tasks_number; i++){

		if (app->tasks[i].id == task_id){

			app->tasks[i].status = ALLOCATED;

			task_allocated++;

		} else if ( app->tasks[i].status == ALLOCATED){
			task_allocated++;
		}
	}

	return task_allocated;
}

/** Set a task status as terminated
 * \param app Application pointer of the task
 * \param task_id ID of the terminated task
 */
void set_task_terminated(Application * app, int task_id){

	Task * t_ptr;

	app->terminated_tasks++;

	t_ptr = get_task_ptr(app, task_id);

	t_ptr->status = TERMINATED_TASK;
}

/** Set a task status as migrating
 * \param app Application pointer of the task
 * \param task_id ID of the migrating task
 */
void set_task_migrating(int task_id){

	Task * t_ptr;

	Application * app = get_application_ptr( (task_id >> 8) );

	t_ptr = get_task_ptr(app, task_id);

	t_ptr->status = MIGRATING;
}

/** Set a task status as migrated
 * \param app Application pointer of the task
 * \param task_id ID of the migrated task
 */
void set_task_migrated(int task_id, int new_proc){

	Task * t_ptr;

	Application * app = get_application_ptr( (task_id >> 8) );

	t_ptr = get_task_ptr(app, task_id);

	t_ptr->status = TASK_RUNNING;

	t_ptr->allocated_proc = new_proc;
}



Application * read_and_create_application(unsigned int app_id, unsigned int * ref_address){

	volatile unsigned int task_id;
	Task * tp;
	Application * app = get_application_ptr(-1);

	app->app_ID = app_id;

	app->tasks_number = *(ref_address++);

	for(int k=0; k < app->tasks_number; k++){

		task_id = *(ref_address++);
		task_id = task_id & (unsigned int) 0xFF;

		tp = &app->tasks[task_id];
		tp->allocated_proc = *(ref_address++);
		tp->code_size = *(ref_address++);
		tp->data_size = *(ref_address++);
		tp->bss_size = *(ref_address++);
		/*tp->initial_address =*/ ref_address++;
		tp->dependences_number = 0;
		tp->computation_load = 0;
		tp->id = app_id << 8 | task_id;
		tp->borrowed_master = -1;

		Puts("Creating task: "); Puts(itoa( tp->id )); Puts("\n");
		Puts("	code_size: "); Puts(itoh( tp->code_size )); Puts("\n");
		Puts("	allocated proc: "); Puts(itoa( tp->allocated_proc )); Puts("\n");

		/*for(int j=0; j < MAX_TASK_DEPENDECES; j++){

			tp->dependences[j].task = (app_id << 8) | *(ref_address++);
			tp->dependences[j].flits = *(ref_address++);

			if(tp->dependences[j].task != -1) {
				tp->dependences_number++;
			}
		}*/
	}

	return app;
}

void remove_application(int app_id){

	Application * app = get_application_ptr(app_id);

	app->app_ID = -1;

	app->status = FREE;

	app->tasks_number = 0;

	app->terminated_tasks = 0;
}

void initialize_applications(){

	for(int i=0; i<MAX_CLUSTER_TASKS; i++){

		applications[i].app_ID = -1;

		applications[i].tasks_number = 0;

		applications[i].status = FREE;

		applications[i].terminated_tasks = 0;
	}
}


#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_APPLICATION_H_ */
