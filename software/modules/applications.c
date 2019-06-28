/*!\file applications.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * Implements the function to manage a insertion, accessing, and remotion of an applications structure
 * This modules is only used by manager kernel
 */

#include "applications.h"
#include "utils.h"
#include "../include/plasma.h"

//Globals
Application applications[MAX_CLUSTER_APP];	//!< Store the applications informations, is equivalent to a attribute in OO paradigm


/** Receives and app ID and return the Application pointer for the required app ID.
 * If not found, the kernel entering in a error situation
 *  \param app_id ID of the application
 *  \return Pointer for Application
 */
Application * get_application_ptr(int app_id){

	for(int i=0; i<MAX_CLUSTER_APP; i++){
		if (applications[i].app_ID == app_id){
			return &applications[i];
		}
	}

	putsv("ERROR: Application not found ", app_id);
	while(1);
	return 0;
}

/** Receives and app ID and return the Application pointer for the required app ID.
 * If not found, return 0
 *  \param app_id ID of the application
 *  \return Pointer for Application or 0 if not found
 */
Application * search_application(int app_id){

	for(int i=0; i<MAX_CLUSTER_APP; i++){
		if (applications[i].app_ID == app_id){
			return &applications[i];
		}
	}

	return 0;
}

/** Searches for a Task pointer into an Application instance. If not found, the kernel entering in a error situation
 *  \param app Task's Application pointer
 *  \param task_id ID of the target task
 *  \return Task pointer
 */
Task * get_task_ptr(Application * app, int task_id){

	for(int i=0; i<app->tasks_number; i++){

		if (app->tasks[i].id == task_id){

			return &app->tasks[i];

		}
	}

	putsv("ERROR: Task not found ", task_id);
	while(1);
	return 0;
}

/** Gets the task status
 *  \param app Task's Application pointer, if 0 the function search for the pointer
 *  \param task_id ID of the target task
 *  \return Task's status
 */
int get_task_status(Application *app, int task_id){
	if(!app)
		app = get_application_ptr(task_id >> 8);
	return (get_task_ptr(app, task_id)->status);
}


/**Searches for a task location by walking for all processors within Application array
 * \param task_id Task ID
 * \return The task location, i.e., the processor address that the task is allocated
 */
int get_task_location(int task_id){

	Application * app = get_application_ptr(task_id >> 8);

	return get_task_ptr(app, task_id)->allocated_proc;
}


Application * get_next_RT_app(){

	Application * app;
	static int next_RT_app_index = 0;

	if (next_RT_app_index == MAX_CLUSTER_APP)
		next_RT_app_index = 0;

	app = &applications[next_RT_app_index];

	next_RT_app_index++;

	if (app->app_ID != -1 && app->tasks[0].is_real_time)
		return app;

	return 0;
}

/** Get the oldest application waiting reclustering
 *  \return Selected Application pointer
 */
Application * get_next_pending_app(){

	Application * older_app = 0;

	for(int i=0; i<MAX_CLUSTER_APP; i++){

		//if (	applications[i].status == APP_WAITING_RECLUSTERING 	|| applications[i].status == APP_WAITING_CS ({			){
		if ( applications[i].status == APP_WAITING_RECLUSTERING ){

			if (older_app == 0 || older_app->app_ID > applications[i].app_ID)
				older_app = &applications[i];

		}
	}

	if (!older_app){
		puts("ERROR: no one app waiting reclustering\n");
		while(1);
	}

	return older_app;
}

/** Seach for a given CS_path pointer based on the producer and consumer task
 * \param producer_task Producer task ID
 * \param consumer_task Consumer task ID
 * \return The CS_path pointer
 */
CS_path * search_CS_path(int producer_task, int consumer_task){

	Application * app;
	Task *t;
	CS_path * ctp;

	//Search CTP pointer
	app = get_application_ptr(consumer_task >> 8);

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		if (t->id != producer_task) continue;

		for (int c = 0; c < t->consumer_tasks_number; c++) {

			ctp = &t->consumer_tasks[c];

			if (ctp->id == consumer_task)
				return ctp;
		}
	}

	puts("ERROR - no CTP found\n");
	while(1);
	return 0;
}

/** Search for all communicating tasks of the task_id and sum the total of latency miss
 * \param task_id Target task
 * \return The sum of latency miss for all CTPs
 */
int get_total_latency_miss(int task_id){

	Application * app;
	Task *t;
	CS_path * ctp;
	int latency_miss_sum = 0;

	//Search CTP pointer
	app = get_application_ptr(task_id >> 8);

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		for (int c = 0; c < t->consumer_tasks_number; c++) {

			ctp = &t->consumer_tasks[c];

			//Only considers the packet switching communication where CS_path == -1 (CS_NOT_ALLOCATED)
			if (ctp->CS_path == -1 && ctp->latency_miss > 2 && (t->id == task_id || ctp->id == task_id))
				latency_miss_sum += ctp->latency_miss;
		}
	}

	return latency_miss_sum;
}

void clear_app_deadline_miss(int app_id){

	Task * t_ptr;
	Application * app_ptr = get_application_ptr(app_id);

	for (int i=0; i<app_ptr->tasks_number; i++){
		t_ptr = &app_ptr->tasks[i];
		t_ptr->deadline_miss = 0;
	}

}

/** Clear deadline and latency miss
 *	\param task_id ID of the target task
 */
void clear_latency_and_deadline_miss(int task_id){

	Application * app_ptr;
	Task * t_ptr;
	CS_path * ctp;

	app_ptr = get_application_ptr(task_id >> 8);
	t_ptr = get_task_ptr(app_ptr, task_id);

	t_ptr->deadline_miss = 0;

	for (int i=0; i<app_ptr->tasks_number; i++){

		t_ptr = &app_ptr->tasks[i];

		for (int c = 0; c < t_ptr->consumer_tasks_number; c++) {

			ctp = &t_ptr->consumer_tasks[c];

			if (t_ptr->id == task_id || ctp->id == task_id){
				ctp->latency_miss = 0;
			}
		}
	}

}

/** Set a new deadline miss
 *	\param task_id ID of the target task
 */
void set_deadline_miss(int task_id){

	Application * app_ptr;
	Task * t_ptr;

	app_ptr = get_application_ptr(task_id >> 8);
	t_ptr = get_task_ptr(app_ptr, task_id);

	t_ptr->deadline_miss++;
}

/** Set a new latency miss
 *	\param task_id ID of the target task
 */
void set_latency_miss(int producer_task, int consumer_task){

	CS_path * ctp = search_CS_path(producer_task, consumer_task);

	ctp->latency_miss++;
}

/** Gets the deadline miss rate
 *	\param task_id ID of the target task
 *	\param task_period Period of the target task
 *	\return The deadline miss rate
 */
int get_deadline_miss_rate(Task * t_ptr){

	int deadline_miss_rate;

	/*current_time = MemoryRead(TICK_COUNTER);

	//putsv("\nCurrent time = ", current_time);
	putsv("Task period = ", t_ptr->period);
	putsv("Task id: ", t_ptr->id);
	putsv("Deadline miss = ", t_ptr->deadline_miss);

	periods_passed = (QOS_MONITORING_WINDOW / t_ptr->period);

	//Gets the period percentage without deadline
	deadline_miss_rate = t_ptr->deadline_miss * 100 / periods_passed;

	putsv("Periods passed = ", periods_passed);

	putsv("Deadline miss rate = ", deadline_miss_rate);
	t_ptr->deadline_miss = 0;*/

	deadline_miss_rate = t_ptr->deadline_miss;
	t_ptr->deadline_miss = 0;

	return deadline_miss_rate;
}

/** Updates the task RT constraints: period and utilization
 * \param task_id ID of the target task
 * \param period Period of the target task
 * \param utilization Utilization of the target task
 */
void update_RT_constraints(int task_id, int period, int utilization){
	Application * app_ptr;
	Task * t_ptr;

	//Gets task info
	app_ptr = get_application_ptr(task_id >> 8);
	t_ptr = get_task_ptr(app_ptr, task_id);

	//Sets the app hyperperiod
	app_ptr->hyperperiod = period;

	t_ptr->period = period;
	t_ptr->utilization = utilization;
}

/** Set a task status as allocated and verifies the number of allocated task for the application
 * \param app Application pointer of the task
 * \param task_id ID of the allocated task
 * \return The number of allocated task for the application
 */
void set_task_allocated(Application * app, int task_id){

	for(int i=0; i<app->tasks_number; i++){

		if (app->tasks[i].id == task_id){

			app->tasks[i].status = ALLOCATED;

			app->allocated_tasks++;

			break;
		}
	}
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

	t_ptr->status = MIGRATING_TASK;
}

/** Set a task status as migrated
 * \param app Application pointer of the task
 * \param task_id ID of the migrated task
 * \return 1|O - 1 to when the task was in the MIGRATING_TASK status, or 0 is when the task is in TASK_RUNNING status
 */
int set_task_migrated(int task_id, int new_proc){

	Task * t_ptr;

	Application * app = get_application_ptr( (task_id >> 8) );

	t_ptr = get_task_ptr(app, task_id);

	if (t_ptr->status == MIGRATING_TASK){

		t_ptr->status = TASK_RUNNING;

		t_ptr->allocated_proc = new_proc;

		return (t_ptr->utilization << 16 | 1);
	}

	return 0;
}



Application * read_and_create_application(unsigned int app_id, volatile unsigned int * ref_address){

	volatile unsigned int task_id;
	Task * tp;
	Application * app = get_application_ptr(-1);

	app->app_ID = app_id;

	app->start_time = MemoryRead(TICK_COUNTER);

	app->last_complete_checkup = 0;

	app->hyperperiod = 0;

	app->allocated_tasks = 0;

	app->tasks_number = *(ref_address++);

	for(int k=0; k < app->tasks_number; k++){

		task_id = *(ref_address++);
		task_id = task_id & (unsigned int) 0xFF;

		tp = &app->tasks[task_id];
		tp->code_size = *(ref_address++);
		tp->data_size = *(ref_address++);
		tp->bss_size = *(ref_address++);
		tp->initial_address = *(ref_address++);
		tp->allocated_proc = -1;
		tp->consumer_tasks_number = 0;
		tp->is_real_time = *(ref_address++);
		tp->id = app_id << 8 | task_id;
		tp->borrowed_master = -1;
		tp->deadline_miss = 0;
		tp->period = 0;
		tp->utilization = 0;
		tp->last_quick_checkup = 0;
		tp->communication_profile = 0;
		tp->communication_profile = 0;
		tp->pending_TM = 0;

		for(int j=0; j < MAX_TASK_DEPENDECES; j++){

			tp->consumer_tasks[j].id = (app_id << 8) | *(ref_address++);
			tp->consumer_tasks[j].CS_path = -1;
			tp->consumer_tasks[j].subnet = -1;
			tp->consumer_tasks[j].latency_miss = 0;

			if(tp->consumer_tasks[j].id != -1) {
				tp->consumer_tasks_number++;
			} else {
				tp->consumer_tasks[j].id = -1;
			}
		}
	}

	return app;
}

void remove_application(int app_id){

	Application * app = get_application_ptr(app_id);

	app->app_ID = -1;

	app->status = APP_FREE;

	app->tasks_number = 0;

	app->terminated_tasks = 0;
}

void initialize_applications(){

	for(int i=0; i<MAX_CLUSTER_APP; i++){

		applications[i].app_ID = -1;

		applications[i].tasks_number = 0;

		applications[i].status = APP_FREE;

		applications[i].terminated_tasks = 0;
	}
}
