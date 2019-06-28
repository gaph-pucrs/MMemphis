/*!\file applications.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Defines structures Application, Task, and Dependence.
 *
 * \detailed The Application structure is useful to the kernel_master to manage the applications' information.
 * This structure is also composed of the substructures: Task and Dependece.
 * Task stores the specific information of each applications's task
 * Dependence stores the task communication dependence information.
 */

#ifndef _APPLICATIONS_H_
#define _APPLICATIONS_H_
#include "../../include/kernel_pkg.h"

//Application status
#define	 APP_RUNNING				1	//!< Signals that the application have all its task mapped and the task already has been requested
#define	 APP_FREE					2	//!< Signals that all task of the application finishes
#define	 APP_WAITING_RECLUSTERING	3	//!< Signals that the application have at least one task waiting for reclustering
#define	 APP_WAITING_CS				4	//!< Signals that the application have at least one task waiting for reclustering
#define	 APP_READY_TO_RELEASE		5	//!< Signals that the application have all its task mapped but the task not were requested yet

//Task status
#define	 REQUESTED				0	//!< Signals that the task has already requested to the global master
#define	 ALLOCATED				1	//!< Signals that the task has successfully allocated into a processor
#define	 TASK_RUNNING			2	//!< Signals that the task is running on the processor
#define	 TERMINATED_TASK		3	//!< Signals that the task terminated its execution
#define	 MIGRATING_TASK			4	//!< Signals that the task was set to be migrated on the slave kernel

#define MAX_TASK_DEPENDECES		10	//!< Stores maximum number of dependences task that a task can have

typedef struct {
	int id;
	int CS_path;
	int subnet;
	int latency_miss;
} CS_path;


/**
 * \brief This structure stores variables useful to manage a task from a manager kernel perspective
 * \detailed Some of the values are loaded from repository others, changed at runtime
 */
typedef struct {
	int  id;						//!< Stores the task id
	int  code_size;					//!< Stores the task code size - loaded from repository
	int  data_size;					//!< Stores the DATA memory section size - loaded from repository
	int  bss_size;					//!< Stores the BSS memory section size - loaded from repository
	int  initial_address;			//!< Stores the initial address of task into repository - loaded from repository
	int  allocated_proc;			//!< Stores the allocated processor address of the task
	int  is_real_time;				//!< Stores if the task is real-time
	int  borrowed_master;			//!< Stores the borrowed master address
	int  status;					//!< Stores the status
	int  consumer_tasks_number;		//!< Stores number of consumer tasks
	int  deadline_miss;				//!< Stores number of deadline misses
	int  period;					//!< Stores the task RT period
	int	 utilization;				//!< Stores the task CPU utilization
	int  last_quick_checkup;		//!< Stores the time of the last quick checkup
	int  computation_profile;		//!< Stores the expected percentage from the computation profile
	int  communication_profile;		//!< Stores the expected percentage from the communication profile
	int  pending_TM;				//!< Stores a flag (0 | 1) indicating when task migration is pending
	CS_path  consumer_tasks[10];	//!< Stores consumer tasks ID array with a size equal to MAX_TASK_DEPENDECES
} Task;

/** \brief This structure store variables useful to the kernel master manage an application instance
 */
typedef struct {
	int app_ID;					//!< Stores the application id
	int status;					//!< Stores the application status
	int	start_time;				//!< Stores the time that application starts its execution
	int tasks_number;			//!< Stores the application task number
	int allocated_tasks;		//!< Stores the number of task allocated of an application
	int terminated_tasks;		//!< Stores the number of task terminated of an application
	int last_complete_checkup;	//!< Stores the time of the last complete checkup
	int hyperperiod;			//!< Stores the app hyperperiod
	Task tasks[MAX_TASKS_APP];	//!< Array of Task with the size equal to the MAX_TASKS_APP
} Application;


Application * get_application_ptr(int);

Application * search_application(int);

CS_path * search_CS_path(int, int);

Task * get_task_ptr(Application * app, int);

Application * get_next_pending_app();

void set_task_allocated(Application *, int);

void set_task_terminated(Application *, int);

void set_task_migrating(int);

int set_task_migrated(int, int);

void update_RT_constraints(int, int, int);

unsigned int get_app_id_counter();

Application * read_and_create_application(unsigned int, volatile unsigned int *);

void remove_application(int);

void initialize_applications();

int get_task_status(Application * , int);

Application * get_next_RT_app();

int get_task_location(int);

void set_deadline_miss(int);

void set_latency_miss(int, int);

int get_deadline_miss_rate(Task *);

void clear_latency_and_deadline_miss(int);

void clear_app_deadline_miss(int);

int get_total_latency_miss(int);

#endif /* SOFTWARE_INCLUDE_APPLICATIONS_APPLICATIONS_H_ */
