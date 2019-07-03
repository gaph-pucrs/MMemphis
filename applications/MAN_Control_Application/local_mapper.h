/*
 * local_mapper.h
 *
 *  Created on: Jul 3, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_H_


#define MAX_TASKS_APP		10


/*Structures*/
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
	int  is_real_time;				//!< Stores if the task is real-time
	int  borrowed_master;			//!< Stores the borrowed master address
	int  status;					//!< Stores the status
	int  deadline_miss;				//!< Stores number of deadline misses
	int  period;					//!< Stores the task RT period
	int	 utilization;				//!< Stores the task CPU utilization
} Task;

/** \brief This structure store variables useful to the kernel master manage an application instance
 */
typedef struct {
	int app_ID;					//!< Stores the application id
	int status;					//!< Stores the application status
	int tasks_number;			//!< Stores the application task number
	int allocated_tasks;		//!< Stores the number of task allocated of an application
	int terminated_tasks;		//!< Stores the number of task terminated of an application
	Task tasks[MAX_TASKS_APP];	//!< Array of Task with the size equal to the MAX_TASKS_APP
} Application;





#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_LOCAL_MAPPER_H_ */
