/*!\file task_location.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * This module implements function relative to task location structure. This module is used by the slave kernel
 * \detailed
 * The task location gives to the slave kernel, the location (slave process address) of the other task
 */

#include "enforcer_mapping.h"
#include "../../hal/mips/HAL_kernel.h"
#include "../../include/services.h"
#include "task_communication.h"
#include "utils.h"

TaskLocation task_location[MAX_TASK_LOCATION];	//!<array of TaskLocation

/**Initializes task_location array with invalid values
 */
void init_task_location(){
	for(int i=0; i<MAX_TASK_LOCATION; i++) {
		task_location[i].id = -1;
		task_location[i].proc_address = -1;
	}
}

/**Searches for the location of a given task
 * \param task_ID The ID of the task
 * \return The task location (processor address in XY)
 */
int get_task_location(int task_ID){

	for(int i=0; i<MAX_TASK_LOCATION; i++) {
		if (task_location[i].id == task_ID){
			return task_location[i].proc_address;
		}
	}
	return -1;
}

/**Add a task_locaiton instance
 * \param task_ID Task ID
 * \param proc Location (address) of the task
 */
void add_task_location(int task_ID, int proc){

	//Ensure to remove all older instances of locations for this task_id
	for(int i=0; i<MAX_TASK_LOCATION; i++) {
		if (task_location[i].id == task_ID){
			task_location[i].id = -1;
			task_location[i].proc_address = -1;
		}
	}

	//Insert the task location in the first free position
	for(int i=0; i<MAX_TASK_LOCATION; i++) {
		if (task_location[i].id == -1){
			task_location[i].id = task_ID;
			task_location[i].proc_address = proc;
			puts("Add task location - task id "); puts(itoa(task_ID)); puts(" proc "); puts(itoh(proc)); puts("\n");
			return;
		}
	}

	puts("ERROR - no FREE Task location\n");
	while(1);
}

/**Remove a task_locaton instance
 * \param task_id Task ID of the instance to be removed
 * \return The location of the removed task, -1 if ERROR
 */
int remove_task_location(int task_id){

	int r_proc;

	for(int i=0; i<MAX_TASK_LOCATION; i++) {

		if (task_location[i].id == task_id){

			task_location[i].id = -1;

			r_proc = task_location[i].proc_address;

			task_location[i].proc_address = -1;
			//puts("Add task location - task id "); puts(itoa(task_ID)); puts(" proc "); puts(itoh(proc)); puts("\n");
			return r_proc;
		}
	}

	puts("ERROR - task not found to remove\n");
	while(1);
	return -1;
}

/**Clear/remove all task of the same application
 * \param app_ID Application ID
 */
void clear_app_tasks_locations(int app_ID){

	for(int i=0;i<MAX_TASK_LOCATION; i++){
		if (task_location[i].id != -1 && (task_location[i].id >> 8) == app_ID){
			//puts("Remove task location - task id "); puts(itoa(task_location[i].id)); puts(" proc "); puts(itoh(task_location[i].proc_address)); puts("\n");
			task_location[i].id = -1;
			task_location[i].proc_address = -1;
		}
	}
}

/** Relase task to execute after mapping is complete by task mapping manager. Sends information
 * about the another task location. The function works in two mode, the first one, with the flag 'from_noc' equal to 1
 * means that the order comes from the NoC, them the packet needs to be copied from DMNI. If 'from_noc' is 0 then
 * the function was called by the management task API
 */
void set_task_release(unsigned int source_addr, char from_noc){

	int app_ID;
	volatile unsigned int app_tasks_location[MAX_TASKS_APP];
	TCB * tcb_ptr;
	unsigned int * data_addr;
	unsigned int task_id, data_size, bss_size, app_task_number;

	/*Code section that extracts the data from the appropriated source*/
	if (from_noc){// Test if the order comes from NoC
		ServiceHeader * p = (ServiceHeader *) source_addr;

		task_id 		= p->task_ID;
		data_size 		= p->data_size;
		bss_size 		= p->bss_size;
		app_task_number = p->app_task_number;

		DMNI_read_data( (unsigned int) app_tasks_location, app_task_number);

		//Sets data_addr to points to the app_task_location array
		data_addr = (unsigned int *) app_tasks_location;

	} else {

		puts("Release manual\n");

		data_addr = (unsigned int *) source_addr;

		task_id 		= data_addr[3];//This index is at same position than p->task_ID
		data_size 		= data_addr[9];//This index is at same position than p->data_size
		bss_size 		= data_addr[11];//This index is at same position than p->bss_size
		app_task_number = data_addr[8];//This index is at same position than p->app_task_number

		//Sets data_addr to points to part of array where the the app_task_location are
		data_addr = &data_addr[CONSTANT_PKT_SIZE];

	}

	/*putsv("Task ID: ", task_id);
	putsv("data_size: ", data_size);
	putsv("bss_size: ", bss_size);
	putsv("app_task_number: ", app_task_number);*/

	/*Code section that actually performs the task release functionality*/

	tcb_ptr = searchTCB(task_id);

	app_ID = task_id >> 8;

	tcb_ptr->data_lenght = data_size;

	//puts("Data lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

	tcb_ptr->bss_lenght = bss_size;

	//puts("BSS lenght: "); puts(itoh(tcb_ptr->data_lenght)); puts("\n");

	tcb_ptr->text_lenght = tcb_ptr->text_lenght - tcb_ptr->data_lenght;

	if (tcb_ptr->scheduling_ptr->status == BLOCKED){
		tcb_ptr->scheduling_ptr->status = READY;
	}

	for (int i = 0; i < app_task_number; i++){
		add_task_location(app_ID << 8 | i, data_addr[i]);
		puts("Add task "); puts(itoa(app_ID << 8 | i)); puts(" loc "); puts(itoh(data_addr[i])); puts("\n");
	}

}

/** Assembles and sends a TASK_ALLOCATED packet to the master kernel
 *  \param allocated_task Allocated task TCB pointer
 */
void send_task_allocated(TCB * allocated_task){

	unsigned int master_addr, master_task_id;
	unsigned int message[2];

	//Gets the task ID
	master_task_id = allocated_task->master_address >> 16;
	//Gets the task localion
	master_addr = allocated_task->master_address & 0xFFFF;

	message[0] = TASK_ALLOCATED;
	message[1] = allocated_task->id;

	if (master_addr == net_address){

		puts("Escrita local: send_task_allocated\n");
		write_local_service_to_MA(master_task_id, message, 2);

	} else {

		send_service_to_MA(master_task_id, master_addr, message, 2);

		while(HAL_is_send_active(PS_SUBNET));

		puts("Sending task allocated\n");
	}

}


/** Assembles and sends a TASK_TERMINATED packet to the master kernel
 *  \param terminated_task Terminated task TCB pointer
 */
//void send_task_terminated(TCB * terminated_task, int perc){/*<--- perc**apagar trecho de end simulation****/
void send_task_terminated(TCB * terminated_task){

	unsigned int message[3];
	unsigned int master_addr, master_id;

	message[0] = TASK_TERMINATED;
	message[1] = terminated_task->id; //p->task_ID
	message[2] = terminated_task->master_address;

	master_addr = terminated_task->master_address & 0xFFFF;
	master_id = terminated_task->master_address >> 16;

	if (master_addr == net_address){

		puts("Escrita local: send_task_terminated\n");

		write_local_service_to_MA(master_id, message, 3);

	} else {

		send_service_to_MA(master_id, master_addr, message, 3);

		puts("Sent task TERMINATED to "); puts(itoh(master_addr)); puts("\n");
		putsv("Master id: ", master_id);

		while(HAL_is_send_active(PS_SUBNET));
	}
	puts("Sended\n");

}

void handle_task_allocation(volatile ServiceHeader * pkt){

	TCB * tcb_ptr;
	unsigned int code_lenght;

	tcb_ptr = search_free_TCB();

	tcb_ptr->pc = 0;

	tcb_ptr->id = pkt->task_ID;

	puts("Task id: "); puts(itoa(tcb_ptr->id)); putsv(" allocated at ", HAL_get_tick());

	code_lenght = pkt->code_size;

	tcb_ptr->text_lenght = code_lenght;

	tcb_ptr->master_address = pkt->master_ID;

	puts("Master address: "); puts(itoh(tcb_ptr->master_address)); puts("\n");

	tcb_ptr->remove_ctp = 0;

	tcb_ptr->add_ctp = 0;

	tcb_ptr->is_service_task = 0;

	tcb_ptr->proc_to_migrate = -1;

	tcb_ptr->scheduling_ptr->remaining_exec_time = MAX_TIME_SLICE;

	DMNI_read_data(tcb_ptr->offset, code_lenght);

	if ((tcb_ptr->id >> 8) == 0){//Task of APP 0 (mapping) dont need to be released to start its execution
		tcb_ptr->scheduling_ptr->status = READY;
	} else { //For others task
		tcb_ptr->scheduling_ptr->status = BLOCKED;
		send_task_allocated(tcb_ptr);
	}
}



