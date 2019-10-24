/*!\file services.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Edited by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief  Kernel services definitions. This services are used to
 * identifies a packet.
 */

#ifndef __SERVICES_H__
#define __SERVICES_H__

#define 	MESSAGE_REQUEST					0x00000010 //Inter-task communication:	Message sent from the consumer task to the producer task requesting a message
#define 	MESSAGE_DELIVERY				0x00000020 //Inter-task communication: 	Message sent from the producer task to the consumer task delivering the requested message
#define 	TASK_ALLOCATION     			0x00000040 //Mapping: 				   	Message sent from the AppInjector to a given slave PE containing the task obj code
#define 	TASK_ALLOCATED     				0x00000050 //Mapping: 				   	Message sent from a slave PE to the LM (Local Mapper), reporting the it receives the TASK_ALLOCATION message and the task was loaded into the memory
#define 	TASK_TERMINATED     			0x00000070 //Mapping: 				   	Message sent from a slave PE to the LM when a user's task finishes it execution
#define 	LOAN_PROCESSOR_RELEASE			0x00000090 //Mapping (Reclustering): 	Message sent from an LM to other LM releasing a borrowed resource (a resource is a memory page in Memphis)
#define 	APP_ALLOCATED					0x00000120 //Mapping:					Message sent from an LM to GM (Global Mapper) informing that the application was successfully mapped and loaded into the slave PEs
#define	 	APP_TERMINATED					0x00000140 //Mapping:					Message sent from an LM to GM informing that all tasks of a given application already finished its execution
#define		NEW_APP							0x00000150 //Mapping:					Message sent from AppInjector to LM containing the application description, i.e, the information of all tasks of the new application requested to be allocated into the LM cluster
#define		INITIALIZE_SLAVE				0x00000170 //TODO Boot:					Message sent from LM to slaves PEs informing that it is the LM of the current slave PE
#define		TASK_TERMINATED_OTHER_CLUSTER	0x00000180 //Mapping (Reclustering):	Message sent from LM to LM notifying when a task from another cluster manager terminated into one of slave PEs of its cluster scope
#define		LOAN_PROCESSOR_REQUEST			0x00000190 //Mapping (Reclustering):	Message sent from an LM to other LM requesting a borrowed resource
#define 	LOAN_PROCESSOR_DELIVERY			0x00000200 //Mapping (Reclustering):	Message sent from an LM to other LM notifying if the requested borrowed resource can be given or not
#define 	TASK_MIGRATION					0x00000210 //TODO Mapping (Migration):	Message sent from an LM to a slave PE requesting to a given task to be migrated from that PE
#define 	MIGRATION_CODE					0x00000220 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task memory code (obj code) data
#define 	MIGRATION_TCB					0x00000221 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task TCB data
#define 	MIGRATION_TASK_LOCATION			0x00000222 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task task locations
#define 	MIGRATION_MSG_REQUEST			0x00000223 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task message request table
#define 	MIGRATION_STACK					0x00000224 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task memory stack data
#define 	MIGRATION_DATA_BSS				0x00000225 //Mapping (Migration):		Message sent from the source slave PE to the target slave PE transmitting the task memory bss data
#define 	UPDATE_TASK_LOCATION			0x00000230 //TODO Mapping (Migration):	Message sent from the source slave PE to the target slave PE updating the location of a migrated task
#define 	TASK_MIGRATED					0x00000235 //TODO Mapping (Migration):	Message sent from from the slave PE to LM reporting that the task migration protocol finished
#define 	APP_ALLOCATION_REQUEST			0x00000240 //Mapping:					Message sent from LM to AppInjetor requesting that it starts to tranferr the tasks object code to the mapped slave PE
#define 	TASK_RELEASE					0x00000250 //Mapping:					Message sent from a LM to slave PE releasing a given task to execute
#define 	SLACK_TIME_REPORT				0x00000260 //Monitoring:				Message sent from a slave PE to LM updating the percentage of idle time of the CPU
#define 	DEADLINE_MISS_REPORT			0x00000270 //Monitoring:				Message sent from a slave PE to LM notifying a deadline miss from a given real-time task
#define 	LATENCY_MISS_REPORT				0x00000275 //Monitoring:				Message sent from a slave PE to LM notifying a latency miss from a given real-time task
#define 	RT_CONSTRANTS					0x00000280 //Real-Time Scheduler:		Message sent from a slave PE to LM informing that a given real-time task update its constraints
#define 	RT_CONSTRANTS_OTHER_CLUSTER		0x00000285 //Real-Time Scheduler:		Message sent from a slave PE (from other cluster) to LM informing that a given real-time task update its constraints
#define		NEW_APP_REQ						0x00000290 //Mapping:					Message sent from AppInjector to GM informing that a new application is requesting to execute
#define		APP_REQ_ACK						0x00000300 //Mapping:					Message sent from GM to AppInjector informing that the application was mapped in a given cluster and AppInjector can transfer the application descriptor to that cluster
#define 	CLEAR_CS_CTP					0x00000320 //TODO QoS (Circuit-Switching):	Message sent from a QoS Manager to slave PE notifying to a CTP (Communicating Task Pair) to stop to communicate using CS
#define 	SET_NOC_SWITCHING_CONSUMER		0x00000360 //QoS (Circuit-Switching):	Message sent from QoS Manager to slave PE of consumer task requesting that a CTP starts its communication using CS
#define 	SET_NOC_SWITCHING_PRODUCER		0x00000370 //QoS (Circuit-Switching):	Message sent from slave PE of consumer task to slave PE of producer task requesting to start to use CS to communicate
#define 	NOC_SWITCHING_PRODUCER_ACK		0x00000380 //QoS (Circuit-Switching):	Message sent from slave PE of producer task to slave PE of consumer task acknowledging the reception of SET_NOC_SWITCHING_PRODUCER
#define 	NOC_SWITCHING_CTP_CONCLUDED		0x00000390 //QoS (Circuit-Switching):	Message sent from slavE PE of producer task to the QoS manager informing about the conclusion of the dynamic CS establishment
#define 	LEARNED_TASK_PROFILE			0x00000420 //DAPE(Dynamic App. Profiling Extraction): Message	sent from slave PE to the DATE manager updating the profiling of a given task
#define		APP_MAPPING_COMPLETE			0x00000440 //Mapping:					Message sent from GM to AppInjector notifying that current application was mapped and AppInjector can send a new application request if it exists

/*Management Application Services*/
#define		INIT_I_AM_ALIVE					0x00000450 //Management Task:			Message sent from an MA task to GM notifying that it was successfully loaded into a given PE
#define		INITIALIZE_MA_TASK				0x00000460 //Management Task:			Message sent from GM to an MA task initializing it

/*SDN Services*/
//SDN - SDN task services
#define 	DETAILED_ROUTING_REQUEST		0x00001001 //Message sent from the coordinator controller to the other controller that belongs to the paths
#define 	DETAILED_ROUTING_RESPONSE		0x00001002 //Message sent from the controllers to the coordinator controller informing about the success of failure of detailed routing
#define 	TOKEN_REQUEST					0x00001003 //Message sent from the coordinator controller to the token coordinator asking for the token grant
#define 	TOKEN_RELEASE					0x00001004 //Message sent from the coordinator releasing the token
#define 	TOKEN_GRANT						0x00001005 //Message sent from the token coordinator passing the token to the requesting coordinator
#define 	UPDATE_BORDER_REQUEST			0x00001006 //Message sent from the coordinator to all controller updating the status of border input
#define 	UPDATE_BORDER_ACK				0x00001007 //Message sent from any controller to its respective coordinator information that it already update the status of border
#define 	LOCAL_RELEASE_REQUEST			0x00001009 //Message sent from a controller to another in order to release all router of the path
#define 	LOCAL_RELEASE_ACK				0x00001010 //Message sent from each controller to the coordinator, informing that it release the path and passing which router was released
#define 	GLOBAL_MODE_RELEASE				0x00001011 //Message sent from the coordinator to all controller canceling the its detailed routing
#define 	GLOBAL_MODE_RELEASE_ACK			0x00001012 //Message sent from each controller to the coordinator informing that it received the order to cancel the detailed routing

//External - SDN services
#define		PATH_CONNECTION_REQUEST			0x00001020 //Message sent from a given component to the SDN controller requesting a SDN path establishment
#define		PATH_CONNECTION_RELEASE			0x00001021 //Message sent from a given component to the SDN controller requesting a SDN path release
#define 	PATH_CONNECTION_ACK				0x00001022 //Message sent from the SDN controller to a given component notifying that the path was established or not
#define		NI_STATUS_REQUEST				0x00001023 //Message sent from the QoS manager to SDN controller requesting the CS allocation status of its DMNI
#define 	NI_STATUS_RESPONSE				0x00001024 //Message sent from the SDN controller to QoS manager replying the NI_STATUS_REQUEST

#define 	SET_CS_ROUTER					0x00001025 //This service is never used, it only exist to allows the Deloream (Graphical Debugger) correctly represent the CS routers setup


#endif
