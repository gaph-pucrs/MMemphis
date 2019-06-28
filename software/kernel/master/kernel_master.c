/*!\file kernel_master.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * Kernel master is the system manager kernel.
 *
 * \detailed
 * kernel_master is the core of the OS running into the managers processors (local and global).
 * It assumes two operation modes: global or local. The operation modes is defined by the global variable is_global_master.
 * Local operation mode: runs into local managers. Manage the applications and task mapping.
 * Global operation mode: runs into global manager. Runs all functions of the local operation mode, further the applications admission control.
 * The kernel_master file uses several modules that implement specific functions
 */

#include "kernel_master.h"

#include "../../modules/utils.h"
#include "../../include/service_api.h"
#include "../../include/services.h"
#include "../../modules/packet.h"
#include "../../modules/new_task.h"
#include "../../modules/resource_manager.h"
#include "../../modules/reclustering.h"
#include "../../modules/applications.h"
#include "../../modules/processors.h"
#include "../../modules/search_path.h"
#include "../../modules/cs_configuration.h"
#include "../../modules/qos_self_awareness_core.h"
#include "../../modules/qos_self_adaptation.h"
#include "../../modules/qos_learning.h"


/*Local Manager (LM) global variables*/
unsigned int 	pending_app = 0; 							//!< Controls the number of pending applications already handled by not completely mapped
unsigned char 	is_global_master;							//!< Defines if this kernel is at global or local operation mode
unsigned int 	global_master_address;						//!< Used to stores the global master address, is useful in local operation mode
unsigned int 	terminated_task_master[MAX_TASKS_APP];		//!< Auxiliary array that stores the terminated task list

/*Global Master (GM) global variables*/
unsigned int 	total_mpsoc_resources = (MAX_LOCAL_TASKS * MAX_SLAVE_PROCESSORS);	//!< Controls the number of total slave processors pages available. Is the admission control variable
unsigned int 	terminated_app_count = 0;											//!< Used to fires the END OF ALL APPLIATIONS
unsigned int 	waiting_app_allocation = 0;											//!< Signal that an application is not fully mapped


/** Receive a address and return the cluster index of array cluster_info[]
 *  \param x Address x of the manager processor
 *  \param y Address y of the manager processor
 *  \return Index of array cluster_info[]
 */
int get_cluster_ID(int x, int y){

	for (int i=0; i<CLUSTER_NUMBER; i++){
		if (cluster_info[i].master_x == x && cluster_info[i].master_y == y){
			return i;
		}
	}
	puts("ERROR - cluster nao encontrado\n");
	return -1;
}

/** Assembles and sends a APP_TERMINATED packet to the global master
 *  \param app The Applications address
 *  \param terminated_task_list The terminated task list of the application
 */
void send_app_terminated(Application *app, unsigned int * terminated_task_list){

	ServiceHeader *p = get_service_header_slot();

	p->header = global_master_address;

	p->service = APP_TERMINATED;

	p->app_ID = app->app_ID;

	p->app_task_number = app->tasks_number;

	send_packet(p, (unsigned int) terminated_task_list, app->tasks_number);

	while (MemoryRead(DMNI_SEND_ACTIVE));

}

/** Assembles and sends a TASK_ALLOCATION packet to a slave kernel
 *  \param new_t The NewTask instance
 */
void send_task_allocation(NewTask * new_t){

	ServiceHeader *p = get_service_header_slot();

	p->header = new_t->allocated_processor;

	p->service = TASK_ALLOCATION;

	p->master_ID = new_t->master_ID;

	p->task_ID = new_t->task_ID;

	p->code_size = new_t->code_size;

	send_packet(p, (0x10000000 | new_t->initial_address), new_t->code_size);

	//puts("Task allocation send - id: "); puts(itoa(p->task_ID)); puts(" send to proc: "); puts(itoh(p->header)); puts("\n");

}

/** Assembles and sends a TASK_RELEASE packet to a slave kernel
 *  \param app The Application instance
 */
void send_task_release(Application * app){

	ServiceHeader *p;
	unsigned int app_tasks_location[app->tasks_number];

	for (int i =0; i<app->tasks_number; i++){
		app_tasks_location[i] = app->tasks[i].allocated_proc;
	}

	for (int i =0; i<app->tasks_number; i++){

		p = get_service_header_slot();

		p->header = app->tasks[i].allocated_proc;

		p->service = TASK_RELEASE;

		p->app_task_number = app->tasks_number;

		p->task_ID = app->tasks[i].id;

		p->data_size = app->tasks[i].data_size;

		p->bss_size = app->tasks[i].bss_size;

		send_packet(p, (unsigned int) app_tasks_location, app->tasks_number);

		app->tasks[i].status = TASK_RUNNING;

		//putsv("-> send TASK_RELEASE to task ", p->task_ID);
		//puts(" in proc "); puts(itoh(p->header)); puts("\n----\n");
	}

	app->status = APP_RUNNING;

	while(MemoryRead(DMNI_SEND_ACTIVE));

	puts("App "); puts(itoa(app->app_ID)); puts(" released to run\n");
}

/** Send a UPDATE_TASK_LOCATION packet to all task of the application in order to update the proc location
 * of the migrated task.
 * \param task_id ID of the migrated task
 * \param new_task_location New processor addresso of the migrated task
 */
void send_update_task_location(int task_id, int new_task_location){

	ServiceHeader *p;
	int proc_list[MAX_TASKS_APP], size_proc_list, proc;
	Application * app = get_application_ptr(task_id >> 8);

	size_proc_list = 0;

	//Clear proc structure
	for(int i=0; i<MAX_TASKS_APP; i++)
		proc_list[i] = -1;

	//Create a target proc list
	for (int i =0; i<app->tasks_number; i++){

		proc = app->tasks[i].allocated_proc;

		for(int j=0; proc_list[j] != proc; j++){

			if (proc_list[j] == -1){
				proc_list[j] = proc;
				size_proc_list++;
				break;
			}
		}

	}

	//Send the UPDATE_TASK_LOCATION_PACKET
	for (int i =0; i<size_proc_list; i++){

		//puts("Vai enviar para "); puts(itoh(proc_list[i])); puts("\n");

		p = get_service_header_slot();

		p->header = proc_list[i];

		p->service = UPDATE_TASK_LOCATION;

		p->task_ID = task_id;

		p->allocated_processor = new_task_location;

		send_packet(p, 0, 0);

	}
}

/** Assembles and sends a APP_ALLOCATION_REQUEST packet to the global master
 *  \param app The Application instance
 *  \param task_info An array containing relevant task informations
 */
void send_app_allocation_request(Application * app, unsigned int * task_info){

	ServiceHeader *p;

	p = get_service_header_slot();

	p->header = global_master_address;

	p->service = APP_ALLOCATION_REQUEST;

	p->master_ID = net_address;

	p->app_task_number = app->tasks_number;

	//putsv("Send new APP REQUEST to master - id: ", p->task_ID );
	//putsv(" app id: ", app->app_ID);

	send_packet(p, (unsigned int) task_info, app->tasks_number*4);

	while (MemoryRead(DMNI_SEND_ACTIVE));

}

/** Assemble and sends a PE_STATISTICS_RESPONSE to another manager
 *	\param received_packet Received packet pointer
 */
void send_PE_statistics_response(int proc, int requesting_manager){

	ServiceHeader *p;

	p = get_service_header_slot();

	p->header = requesting_manager;

	p->service = PE_STATISTICS_RESPONSE;

	p->requesting_processor = proc;

	p->computation_task_number = get_number_computation_task(proc);

	//Merges both utilization in a single variable
	p->utilization = get_proc_utilization(proc);

	p->task_number = get_RT_task_number(proc);

	send_packet(p, 0, 0);
}

/** Requests a new application to the global master kernel
 *  \param app Application to be requested
 */
void request_application(Application *app){

	Task *t;
	NewTask nt;
	unsigned int task_info[app->tasks_number*4];
	int index_counter;

	//puts("App requested\n");

	index_counter = 0;

	for (int i=0; i<app->tasks_number; i++){

		t = &app->tasks[i];

		if (t->allocated_proc == -1){
			putsv("ERROR task id not allocated: ", t->id);
			while(1);
		}

		t->status = REQUESTED;

		if (is_global_master){

			//Is equivalent to the APP ALLOCATION_REQUEST TREATMENT
			nt.allocated_processor = t->allocated_proc;
			nt.initial_address = t->initial_address;
			nt.code_size = t->code_size;
			nt.master_ID = net_address;
			nt.task_ID = t->id;

			add_new_task(&nt);

		} else {

			task_info[index_counter++] = t->id;
			task_info[index_counter++] = t->allocated_proc;
			task_info[index_counter++] = t->initial_address;
			task_info[index_counter++] = t->code_size;
		}
	}

	if (is_global_master){

		waiting_app_allocation = 0;

	} else {

		send_app_allocation_request(app, task_info);
	}
}

/** Handles a pending application. A pending application it the one which there is some task to be inserted into reclustering
 */
void handle_pending_application(){

	Application *app = 0;
	int request_app;

	/*Selects an application pending to be mapped due reclustering*/
	app = get_next_pending_app();

	if (app->status == APP_WAITING_RECLUSTERING){

		//putsv("Handle next application waiting reclustering - id ", app->app_ID);

		request_app = reclustering_application(app);

		if (request_app){

			request_application(app);

			//Tests if the application is real-time by testing the first app task
			//if (app->tasks[0].is_real_time){
			//	app->status = APP_WAITING_CS;
				//puts("App entering in WAITING_CS status\n");
			//} else {
				app->status = APP_READY_TO_RELEASE;
				pending_app--;
				//putsv("App READY TO RELEASE after reclustering - pending app = ", pending_app);
			//}

		}
	}

	/*if (app->status == APP_WAITING_CS){

		//putsv("Handle next application waiting CS - id ", app->app_ID);

		cs_finishes = request_CS_application(app, 0, 0);

		if (cs_finishes){

			pending_app--;

			if (app->allocated_tasks == app->tasks_number){
				//putsv("App READY TO RELEASE after CS - pending app = ", pending_app);
				send_task_release(app);
			} else {
				app->status = APP_READY_TO_RELEASE;
				//putsv("App READY TO RELEASE after CS 2 - pending app = ", pending_app);
			}

		}
	}*/

}

/** Handles an application which terminated its execution
 *  \param appID Application ID of the terminated app
 *  \param app_task_number Application task number
 *  \param app_master_addr Application master XY address
 */
void handle_app_terminated(int appID, unsigned int app_task_number, unsigned int app_master_addr){

	unsigned int task_master_addr;
	int borrowed_cluster, original_cluster;

	//puts("original master addrr "); puts(itoh(app_master_addr)); puts("\n");

	original_cluster = get_cluster_ID(app_master_addr >> 8, app_master_addr & 0xFF);

	for (int i=0; i<app_task_number; i++){
		task_master_addr = terminated_task_master[i];

		//puts("Terminated task id "); puts(itoa(appID << 8 | i)); puts(" with physycal master addr "); puts(itoh(task_master_addr)); puts("\n");

		if (task_master_addr != app_master_addr && task_master_addr != net_address){

			borrowed_cluster = get_cluster_ID(task_master_addr >> 8, task_master_addr & 0xFF);

			release_cluster_resources(borrowed_cluster, 1);

		} else if (task_master_addr != net_address){ // Because only the global calls this funtion
			//puts("Remove original\n");
			release_cluster_resources(original_cluster, 1);
		}
	}
	total_mpsoc_resources += app_task_number;
	puts("Handle APP terminated - total_mpsoc_resources: "); puts(itoa(total_mpsoc_resources)); puts("\n");
	terminated_app_count++;
	//puts("SIMUL_PROGRESS "); puts(itoa(((terminated_app_count*100)/(APP_NUMBER-1))));puts("%\n");

	//puts("\n-------\n");

	/*if (terminated_app_count == (APP_NUMBER-1)){//-1 because the hadlock_OS_service
		puts("FINISH ");puts(itoa(MemoryRead(TICK_COUNTER))); puts("\n");
		MemoryWrite(END_SIM,1);
	}*/

}

void handle_api_service_message(unsigned int msg_lenght){

	unsigned int data_message[50];
	int path_id = 0;

	DMNI_read_data((unsigned int)data_message, msg_lenght);

	switch (data_message[0]) {

		case PATH_CONNECTION_ACK://Used to set a CS
			//1 because there is no more path ID
			if (data_message[2] == 0){
				path_id = -1;
				puts("PATH FAILURE\n");
			} else
				puts("PATH SUCESSS\n");
			putsv("\nRECEIVED SUBNET: ",data_message[5])
			putsv("LATENCY: ", (MemoryRead(TICK_COUNTER) - sdn_setup_latency));
			set_runtime_CS(path_id, data_message[5]);
			break;
		case NI_STATUS_RESPONSE:
			handle_NI_CS_status(data_message[1], data_message[2]);
			break;
		default:
			puts("ERROR: task OS service unknow\n");
			while(1);
			break;
	}
}

unsigned int total_req_percentage = 0;

/** Handles a new packet from NoC
 */
void handle_packet() {

	int app_id, index_counter, master_addr, aux;
	volatile ServiceHeader p;
	NewTask nt;
	Application *app;
	unsigned int task_info[MAX_TASKS_APP*4];

	read_packet((ServiceHeader *)&p);

	switch (p.service){

	case NEW_APP:

		handle_new_app(p.app_ID, 0, p.app_descriptor_size);

		break;

	case TASK_ALLOCATED:

		app_id = p.task_ID >> 8;

		app = get_application_ptr(app_id);

		set_task_allocated(app, p.task_ID);

		//puts("-> TASK ALLOCATED from task "); puts(itoa(p.task_ID)); putsv("\tAllocated tasks: ", app->allocated_tasks);

		if (app->allocated_tasks == app->tasks_number && app->status == APP_READY_TO_RELEASE){

			send_task_release(app);
		}

		break;

	case INITIALIZE_CLUSTER:

		global_master_address = p.source_PE;

		set_cluster_ID(p.cluster_ID);

		//Reclustering setup
		reclustering_setup();

		//QoS Manager setup
		init_CS_configuration();

		initialize_slaves();

		break;

	case LOAN_PROCESSOR_REQUEST:
	case LOAN_PROCESSOR_DELIVERY:
	case LOAN_PROCESSOR_RELEASE:

		handle_reclustering((ServiceHeader *)&p);

		break;

	case APP_ALLOCATION_REQUEST:

		//puts("New app allocation request\n");

		index_counter = 0;

		DMNI_read_data((unsigned int)task_info, (p.app_task_number*4));

		for(int i=0; i< p.app_task_number; i++){

			nt.task_ID = task_info[index_counter++];
			nt.allocated_processor = task_info[index_counter++];
			nt.initial_address = task_info[index_counter++];
			nt.code_size = task_info[index_counter++];
			nt.master_ID = p.master_ID;

			add_new_task(&nt);

			puts("New task requisition: "); puts(itoa(nt.task_ID)); puts(" allocated proc ");
			puts(itoh(nt.allocated_processor)); puts("\n");

			/*These lines above mantain the cluster resource control at a global master perspective*/
			master_addr = get_master_address(nt.allocated_processor);

			//net_address is equal to global master address, it is necessary to verifies if is master because the master controls the cluster resources by gte insertion of new tasks request
			if (master_addr != net_address && master_addr != nt.master_ID){

				//Reuse of the variable master_addr to store the cluster ID
				master_addr = get_cluster_ID(master_addr >> 8, master_addr & 0xFF);

				//puts("Reservou por reclustering\n");

				allocate_cluster_resource(master_addr, 1);
			}

		}

		waiting_app_allocation = 0;

		break;

	case APP_TERMINATED:

		DMNI_read_data((unsigned int)terminated_task_master, p.app_task_number);

		handle_app_terminated(p.app_ID, p.app_task_number, p.source_PE);

		break;

	case TASK_TERMINATED:

		app_id = p.task_ID >> 8;

		app = get_application_ptr(app_id);

		set_task_terminated(app, p.task_ID);

		putsv("Task TERMINATED, ID = ", p.task_ID);

		/***apagar trecho de end simulation****/
		/*total_req_percentage++;
		putsv("total_req_percentage: ", total_req_percentage);
		if (total_req_percentage >= p.mode){
			puts("End SIMULATION: \n");
			MemoryWrite(END_SIM,1);
		}*/
		/***apagar trecho de end simulation****/
		//apagar tambem variavel total_req_percentage


		release_task_CS(app, p.task_ID);

		//Test if is necessary to terminated the app
		if (app->terminated_tasks == app->tasks_number){

			for (int i=0; i<app->tasks_number; i++){

				if (app->tasks[i].borrowed_master != -1){
					terminated_task_master[i] = app->tasks[i].borrowed_master;
				} else {
					terminated_task_master[i] = net_address;

					if (p.master_ID == net_address)
						page_released(clusterID, app->tasks[i].allocated_proc, app->tasks[i].id);

				}
			}

			if (is_global_master) {

				handle_app_terminated(app->app_ID, app->tasks_number, net_address);

			} else {

				send_app_terminated(app, terminated_task_master);
			}

			remove_application(app->app_ID);
		}

		break;

	case TASK_TERMINATED_OTHER_CLUSTER:

		page_released(clusterID, p.source_PE, p.task_ID);

		break;

	case TASK_MIGRATED:

		//Set the status of the task as running and verifies if the task is already running, if YES return 0, no = 1
		aux = set_task_migrated(p.task_ID, p.allocated_processor);

		//If the task was migrated and the TASK_MIGRATED packet is received at first time
		if (aux) {

			puts("Received task migrated - task id "); puts(itoa(p.task_ID)); puts(" new proc "); puts(itoh(p.allocated_processor)); puts("\n");

			master_addr = get_master_address(p.source_PE);

			//Update the page only if the old processor belongs to task cluster
			if(master_addr == net_address){
				page_released(clusterID, p.source_PE, p.task_ID);
			}

			//Page used already update in send_task_migration()
			//page_used(clusterID, p.allocated_processor, p.task_ID);

			//Update other proc
			send_update_task_location(p.task_ID, p.allocated_processor);

			//Extract the utilization of the task
			aux = aux >> 16;
			//putsv("Task Utilization = ", aux);

			if (aux > 0){

				update_proc_utilization(p.allocated_processor, p.task_ID, aux);

			}

			task_migration_notification(p.task_ID);


		} else
			puts("Packet discarted\n");

		break;

	case SLACK_TIME_REPORT:

		update_proc_slack_time(p.source_PE, p.cpu_slack_time);

		break;

	case SERVICE_TASK_MESSAGE:

		handle_api_service_message(p.msg_lenght);

		break;

	case NOC_SWITCHING_PRODUCER_ACK:

		if (p.cs_mode)//Is is stablishing CS, only one CTP is envolved
			concludes_runtime_CS(p.producer_task, p.consumer_task);
		else
			release_CS_before_migration(0, 0);//Continues the releasing process

		break;

	case DEADLINE_MISS_REPORT:

		puts("\n\n++++++++++ "); puts(itoa(MemoryRead(TICK_COUNTER))); puts(" +++++++++++++++\n");
		puts("\n+++> Deadline miss for task = "); puts(itoa(p.task_ID)); puts("\n");

		//evaluates_deadline_miss(p.task_ID, p.proc_status);

		set_deadline_miss(p.task_ID);

		if (!pending_app)
			QoS_analysis(0, (ServiceHeader *)&p);


		break;

	case LATENCY_MISS_REPORT:

		set_latency_miss(p.producer_task, p.consumer_task);

		puts("\n\n############## "); puts(itoa(MemoryRead(TICK_COUNTER))); puts(" ##############+\n");
		puts("==> Latency miss between "); puts(itoa(p.producer_task)); putsv(" -> ", p.consumer_task);

		if (!pending_app)
			QoS_analysis(0, (ServiceHeader *) &p);

		break;

	case RT_CONSTRANTS:

		//Gets the physical master address of task
		master_addr = get_master_address(p.source_PE);

		//Gets task utilization
		aux = p.utilization >> 16;

		//Update the task utilization at the application level
		update_RT_constraints(p.task_ID, p.period, aux);

		//When the task is running on a foreing cluster it update the processor utilization by calling RT_CONSTRANTS_OTHER_CLUSTER
		if (master_addr == net_address)
			update_proc_utilization(p.source_PE, p.task_ID, aux);

		//Get CPU utilization
		//aux = p.utilization & 0xFFFF;
		//puts("\tRT_CONSTRANTS - task = "); puts(itoa(p.task_ID)); puts(" proc = "); puts(itoh(p.source_PE));  putsv(" proc utilization = ", aux);


		puts("\n\n----------------- "); puts(itoa(MemoryRead(TICK_COUNTER))); puts(" -----------------\n");
		puts("--> RT constraint change for task "); puts(itoa(p.task_ID));  puts("\n");

		if (!pending_app )
			QoS_analysis(0, (ServiceHeader *) &p);

		break;

	case RT_CONSTRANTS_OTHER_CLUSTER:

		update_proc_utilization(p.source_PE, p.task_ID, p.utilization);

		break;

	case PE_STATISTICS_REQUEST:

		send_PE_statistics_response(p.requesting_processor, p.source_PE);

		break;

	case PE_STATISTICS_RESPONSE:

		remote_CPU_data_protocol(p.requesting_processor, p.utilization, p.computation_task_number, p.task_number);

		break;

	case LEARNED_TASK_PROFILE:

		handle_learned_profile_packet(p.task_ID, p.task_profile);

		break;
	default:
		puts("ERROR: service unknown ");puts(itoh(p.service)); puts(" ");
		putsv(" time: ", MemoryRead(TICK_COUNTER));
		break;
	}

}


/** Initializes all slave processor by sending a INITIALIZE_SLAVE packet to each one
 */
void initialize_slaves(){

	ServiceHeader *p;
	int proc_address, index_counter;

	init_procesors();

	index_counter = 0;

	for(int i=cluster_info[clusterID].xi; i<=cluster_info[clusterID].xf; i++) {

		for(int j=cluster_info[clusterID].yi; j<=cluster_info[clusterID].yf; j++) {

			proc_address = i*256 + j;//Forms the proc address

			if( proc_address != net_address) {

				//Fills the struct processors
				add_procesor(proc_address);

				//Sends a packet to the slave
				p = get_service_header_slot();

				p->header = proc_address;

				p->service = INITIALIZE_SLAVE;

				send_packet(p, 0, 0);

				index_counter++;
			}
		}
	}

}

/** Initializes all local managers by sending a INITIALIZE_CLUSTER packet to each one
 */
void initialize_clusters(){

	int cluster_master_address;
	ServiceHeader *p;

	for(int i=0; i<CLUSTER_NUMBER; i++) {

		cluster_master_address = (cluster_info[i].master_x << 8) | cluster_info[i].master_y;

		puts("Initializing cluster ID "); puts(itoa(i)); puts("\n");

		if(cluster_master_address != global_master_address){

			putsv("Address: ", cluster_master_address);

			p = get_service_header_slot();

			p->header = cluster_master_address;

			p->service = INITIALIZE_CLUSTER;

			p->source_PE = global_master_address;

			p->cluster_ID = i;

			send_packet(p, 0, 0);

		} else {

			puts("Global Manager initialized with ID "); puts(itoa(i)); puts("\n");

			set_cluster_ID(i);

			//Reclustering setup
			reclustering_setup();

			//QoS Manager setup
			init_CS_configuration();

		}
	}
}

/** Handles a new application incoming from the global manager or by repository
 * \param app_ID Application ID to be handled
 * \param ref_address Pointer to the application descriptor. It can point to a array (local manager) or the repository directly (global manager)
 * \param app_descriptor_size Size of the application descriptor
 */
void handle_new_app(int app_ID, volatile unsigned int *ref_address, unsigned int app_descriptor_size){

	Application *application;
	int mapping_completed = 0;

	//Cuidado com app_descriptor_size muito grande, pode estourar a memoria
	unsigned int app_descriptor[app_descriptor_size];

	if (!is_global_master){

		DMNI_read_data( (unsigned int) app_descriptor, app_descriptor_size);

		ref_address = app_descriptor;

	}
	//Creates a new app by reading from ref_address
	application = read_and_create_application(app_ID, ref_address);

	pending_app++;
	//putsv("Handle new app - ID = ", app_ID);

	mapping_completed = application_mapping(clusterID, application->app_ID);

	if (mapping_completed){

		request_application(application);

		//if (application->tasks[0].is_real_time){
		//	application->status = APP_WAITING_CS;
			//puts("App APP_WAITING_CS\n");
		//} else {
			application->status = APP_READY_TO_RELEASE;
			pending_app--;
			//puts("App APP_READY_TO_RELEASE afer normal mapping\n");
		//}

	} else {
		puts("Application waiting reclustering\n");
		application->status = APP_WAITING_RECLUSTERING;
	}
}

/** Handles a new application request triggered by test_bench (repository)
 */
void handle_app_request(){

	unsigned int * initial_address;
	unsigned int num_app_tasks, app_descriptor_size, selected_cluster_proc;
	static unsigned int app_id_counter = 0;
	int selected_cluster;
	ServiceHeader *p;

	initial_address = (unsigned int *) (0x10000000 | external_app_reg);

	num_app_tasks = *initial_address;

	if (total_mpsoc_resources < num_app_tasks || waiting_app_allocation){
		//puts("App refused - total_mpsoc_resources = "); puts(itoa(total_mpsoc_resources)); puts(" num_app_tasks = ");puts(itoa(num_app_tasks)); puts("\n");
		return;
	}

	//TASK_DESCRIPTOR_SIZE is the size of each task description into repository -- see testcase_name/repository_debug.txt
	app_descriptor_size = (TASK_DESCRIPTOR_SIZE * num_app_tasks) + 1; //plus 1 because the first address is the app task number

	selected_cluster = SearchCluster(clusterID, num_app_tasks);

	puts("Handle application request from repository - app address: "); puts(itoh((unsigned int)initial_address));

	waiting_app_allocation = 1;

	puts(" number of tasks= "); puts(itoa(num_app_tasks));
	putsv(" total_mpsoc_resources ", total_mpsoc_resources);

	total_mpsoc_resources -= num_app_tasks;

	selected_cluster_proc = get_cluster_proc(selected_cluster);

	if (selected_cluster_proc == global_master_address){

		handle_new_app(app_id_counter, initial_address, app_descriptor_size);

	} else {

		allocate_cluster_resource(selected_cluster, num_app_tasks);

		p = get_service_header_slot();

		p->header = selected_cluster_proc;

		p->service = NEW_APP;

		p->app_ID = app_id_counter;

		p->app_descriptor_size = app_descriptor_size;

		send_packet(p, (unsigned int)initial_address, app_descriptor_size);
	}

	app_id_counter++;

	MemoryWrite(ACK_APP, 1);
}

int main() {

	NewTask * pending_new_task;
	AdaptationRequest * adpt_request_ptr;
	int consumer_processor;

	init_packet();

	puts("Initializing PE: "); puts(itoh(net_address)); puts("\n");

	initialize_applications();

	init_new_task_list();

	init_learning();

	init_QoS_adaptation();

	//By default HeMPS assumes that GM is positioned at address 0
	if ( net_address == 0){

		puts("This kernel is global master\n");

		is_global_master = 1;

		global_master_address = net_address;

		initialize_clusters();

		initialize_slaves();

	} else {

		puts("This kernel is local master\n");

		is_global_master = 0;
	}

	puts("Kernel Initialized\n");

	//int flag = 1;

	for (;;) {

		//Migration trigger
		/*if (net_address == 0 && MemoryRead(TICK_COUNTER) >2500000 && flag){
			flag = 0;
			//send_task_migration(259, 0x102);
			puts("MIGRATION FORCED STARTED\n");
			add_adaptation_request(get_task_ptr(get_application_ptr(259>>8), 259), TaskMigration, 0);
		}*/

		//LM looping
		if (noc_ps_interruption){

			handle_packet();

		} else if (pending_app && is_reclustering_NOT_active() && is_CS_NOT_active()){

			handle_pending_application();

		//GM looping
		} else if (is_global_master && !is_send_active(PS_SUBNET)) {

			pending_new_task = get_next_new_task();

			if (pending_new_task){

				send_task_allocation(pending_new_task);

				pending_new_task->task_ID = -1;

			} else if (app_req_reg) {

				handle_app_request();
			}
		}

		/*putsv("\n!pending_app = ", (!pending_app));
		putsv("is_CS_NOT_active() = ", (is_CS_NOT_active()));
		putsv("!is_QoS_analysis_active() = ", (!is_QoS_analysis_active()));
		puts("\n");*/

		if (!pending_app && is_CS_NOT_active() && !is_QoS_analysis_active()){

			adpt_request_ptr = get_next_adapt_request();

			//Test if there is a pending adaptation request
			if (adpt_request_ptr){

				if (adpt_request_ptr->type == TaskMigration){

					adpt_request_ptr->task->pending_TM = 0;

					migrate_RT(adpt_request_ptr->task);

				} else {

					consumer_processor = get_task_location(adpt_request_ptr->ctp->id);

					request_runtime_CS(adpt_request_ptr->task->id, adpt_request_ptr->ctp->id, adpt_request_ptr->task->allocated_proc, consumer_processor);
				}

			} else //Otherwise perform the routine overhaul
				check_app_routine_overhaul();
		}


	}

	return 0;
}
