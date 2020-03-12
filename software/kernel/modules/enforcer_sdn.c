/*!\file ctp.c
 * HEMPS VERSION - 8.0 - QoS
 *
 * Distribution:  August 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Store the informations for each CTP (Communication Task Pair).
 *
 * \detailed
 * A ctp is used in CS mode. When a task belowing to a ctp need to send or receive a packet, the kernel
 * access this structure to be aware about the other task of the ctp pair
 */
#include "enforcer_sdn.h"

#include "../../hal/mips/HAL_kernel.h"
#include "../../include/services.h"
#include "enforcer_mapping.h"
#include "utils.h"

#define MAX_CTP	(SUBNETS_NUMBER-1) * 2 //!< Maximum number of ctp into a slave processor

CTP ctp[ MAX_CTP ];

void init_ctp(){
	for(int i=0; i<MAX_CTP; i++){
		ctp[i].subnet = -1;
		ctp[i].dmni_op = -1;
		ctp[i].CS_enabled = 0;
	}
}

CTP * get_ctp_ptr(int subnet, int dmni_op){

	for(int i=0; i<MAX_CTP; i++){
		if (ctp[i].subnet == subnet && ctp[i].dmni_op == dmni_op){
			return &ctp[i];
		}
	}
	puts("ERROR: ctp not found - subnet "); puts(itoa(subnet)); putsv(" dmni op ", dmni_op);

	while(1);
	return 0;
}

CTP * add_ctp(int producer_task, int consumer_task, int dmni_op, int subnet){

	for(int i=0; i<MAX_CTP; i++){
		if (ctp[i].subnet == -1){
			ctp[i].producer_task = producer_task;
			ctp[i].consumer_task = consumer_task;
			ctp[i].subnet = subnet;
			ctp[i].dmni_op = dmni_op;
			ctp[i].CS_enabled = 0;

#if CS_DEBUG
			puts("\n-----\nAdded ctp pair   "); puts(itoa(producer_task)); puts(" -> "); puts(itoa(consumer_task));
			puts(" to subnet "); puts(itoa(subnet)); putsv(" dmni op ", dmni_op);
#endif
			return &ctp[i];
		}
	}
	puts("ERROR - no FREE CTP\n");
	while(1);
	return 0;
}

void remove_ctp(int subnet, int dmni_op){

	for(int i=0; i<MAX_CTP; i++){
		if (ctp[i].subnet == subnet && ctp[i].dmni_op == dmni_op){

#if CS_DEBUG
			puts("\n------\nRemoved ctp pair   "); puts(itoa(ctp[i].producer_task)); puts(" -> "); puts(itoa(ctp[i].consumer_task));
			putsv(" to subnet ", subnet);
#endif
			ctp[i].subnet = -1;
			ctp[i].dmni_op = -1;
			ctp[i].CS_enabled = 0;
			return;
		}
	}

	puts("ERROR - no CTP to remove\n");
	while(1);
}


int get_subnet(int producer_task, int consumer_task, int dmni_op){
	for(int i=0; i<MAX_CTP; i++){
		if (ctp[i].producer_task == producer_task && ctp[i].consumer_task == consumer_task && ctp[i].dmni_op == dmni_op){
			if (ctp[i].CS_enabled == 0)
				return -1;
			//putsv("Return subnet ", ctp[i].subnet);
			return ctp[i].subnet;
		}
	}
	return -1;
}

/**Add the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This procedure ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_add TCB pointer of the task to allocate the CTP
 */
void add_ctp_online(TCB * task_to_add){

	ServiceHeader * p;
	int producer_task, subnet;
	int producer_address;

	producer_task = task_to_add->add_ctp & 0xFFFF;

	//Removes the old subnet registry
	subnet = get_subnet(producer_task, task_to_add->id, DMNI_RECEIVE_OP);
	if (subnet != -1){
		remove_ctp(subnet, DMNI_RECEIVE_OP);
	}

	subnet = task_to_add->add_ctp >> 16;

	add_ctp(producer_task, task_to_add->id, DMNI_RECEIVE_OP, subnet);

	producer_address = get_task_location(producer_task);

	while (producer_address == -1){
		puts("ERROR, add_ctp_online producer_address -1\n");
	}

	p = get_service_header_slot();

	//p->header = get_task_location(producer_task);
	p->header = producer_address;

	p->service = SET_NOC_SWITCHING_PRODUCER;

	p->producer_task = producer_task;

	p->consumer_task = task_to_add->id;

	p->cs_mode = 1;

	p->cs_net = subnet;

	send_packet(p, 0, 0);

	task_to_add->add_ctp = 0;

	//puts("add_ctp_online: SET_NOC_SWITCHING_PRODUCER sent\n");
}

/**Remove the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This producere ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_remove TCB pointer of the task to remove the CS
 */
void remove_ctp_online(TCB * task_to_remove){

	ServiceHeader * p;
	int producer_address;

	//puts("remove_ctp_online\n");

	int subnet = get_subnet(task_to_remove->remove_ctp, task_to_remove->id, DMNI_RECEIVE_OP);

	while (subnet == -1){
		puts("ERROR, subnet -1\n");
	}

	producer_address = get_task_location(task_to_remove->remove_ctp);

	while (producer_address == -1){
		puts("ERROR, remove_ctp_online producer_address -1\n");
	}

	remove_ctp(subnet, DMNI_RECEIVE_OP);

	p = get_service_header_slot();

	//p->header = get_task_location(task_to_remove->remove_ctp);
	p->header = producer_address;

	p->service = SET_NOC_SWITCHING_PRODUCER;

	p->producer_task = task_to_remove->remove_ctp;

	p->consumer_task = task_to_remove->id;

	p->cs_mode = 0;

	p->cs_net = subnet;

	send_packet(p, 0, 0);

	task_to_remove->remove_ctp = 0;

}

void check_ctp_reconfiguration(TCB * tcb_ptr){
	if (tcb_ptr->add_ctp)
		add_ctp_online(tcb_ptr);
	else if (tcb_ptr->remove_ctp)
		remove_ctp_online(tcb_ptr);
}

/** Assembles and sends a NOC_SWITCHING_PRODUCER_ACK packet to the consumer task, signalizing
 * that the CS release or stablisment was accomplished
 *  \param producer_task Task ID of the CTP's producer task
 *  \param consumer_task Task ID of the CTP's consumer task
 *  \param subnet Subnet ID
 *  \param cs_mode Flag indicating to release or stablish a CS connection (1 - stablish, 0 - release)
 */
void send_NoC_switching_ack(int producer_task, int consumer_task, int subnet, int cs_mode){

	ServiceHeader * p;
	TCB * prod_tcb_ptr;

	prod_tcb_ptr = searchTCB(producer_task);

	if (!prod_tcb_ptr){
		puts("ERROR, not TCB found in send_cs_ack\n");
		for(;;);
	}

	//Sends the ACK to the consumer also
	//if (cs_mode) {//Only send if the mode is establish

	p = get_service_header_slot();

	p->header = get_task_location(consumer_task);

	p->service = NOC_SWITCHING_PRODUCER_ACK;

	p->producer_task = producer_task;

	p->consumer_task = consumer_task;

	p->cs_net = subnet;

	p->cs_mode = cs_mode;

	send_packet(p, 0, 0);

	//}


}
/* Dynamic CS establishment protocol. A order is sent from QoS manager to the consumer task, which sends the order
 * to the producer task, which also changes the communication mode to CS and sends the ACK to consumer and QoS manager.
 * The CS communication only occurs after the consumer task receives the ACK.
  _____________								 _____________								  _____________
 |producer task|							|consumer task|							   	 |QoS  manager |
       |										    |										    |
       |											| <------- SET_NOC_SWITCHING_CONSUMER ------|
       |											|										    |
       | <------ SET_NOC_SWITCHING_PRODUCER --------|										    |
       |											|										    |
       |											|										    |
       | ------ NOC_SWITCHING_PRODUCER_ACK ------->	|											|
       |											| (CS communication starts here)       		|
       |											|										    |					    |
       | 											|------ NOC_SWITCHING_CTP_CONCLUDED ------> |
       |											|										    |
       |											|										    |
 * */

void handle_dynamic_CS_setup(volatile ServiceHeader * p){

	TCB * tcb_ptr = 0;
	CTP * ctp_ptr = 0;
	unsigned int message[4];
	static unsigned int qos_master_id, qos_master_addr;
	int aux_subnet;

	switch (p->service) {

	case SET_NOC_SWITCHING_CONSUMER:

		tcb_ptr = searchTCB(p->consumer_task);

		while(!tcb_ptr) puts("ERROR: tcb is null at SET_NOC_SWITCHING_CONSUMER\n");

		qos_master_id = p->task_number;
		qos_master_addr = p->source_PE;


		if (p->cs_mode) {  //Stablish Circuit-Switching

			//puts("Set CS command received from QoS manager\n"); //Deixar esse comentario, eh importante, removi para metricas de tempo

			tcb_ptr->add_ctp = p->cs_net << 16 | p->producer_task;

		} else {  //Stablish Packet-Switching

			puts("Set PS command received from QoS manager\n");

			tcb_ptr->remove_ctp = p->producer_task;
		}

		//Set ctp producer address with the producer task PE address

		//Test if the task is not waiting for a message
		if (!tcb_ptr->scheduling_ptr->waiting_msg){

			check_ctp_reconfiguration(tcb_ptr);
		}

		break;

	case SET_NOC_SWITCHING_PRODUCER:

		//puts("++++++ SET_NOC_SWITCHING_PRODUCER\n");

		if (p->cs_mode) {
			//puts("CS mode enabled at producer\n"); //Deixar esse comentario, eh importante, removi para metricas de tempo

			//Removes the old subnet registry
			aux_subnet = get_subnet(p->producer_task, p->consumer_task, DMNI_SEND_OP);
			if (aux_subnet != -1){
				remove_ctp(aux_subnet, DMNI_SEND_OP);
			}

			ctp_ptr = add_ctp(p->producer_task, p->consumer_task, DMNI_SEND_OP, p->cs_net);
			ctp_ptr->CS_enabled = 1;
		} else {
			remove_ctp(p->cs_net, DMNI_SEND_OP);
			puts("PS mode enabled at producer\n");
			p->cs_net = 0xF0DAEB0A; //Only to p->cs_net does't carry garbage
		}

		send_NoC_switching_ack(p->producer_task, p->consumer_task, p->cs_net, p->cs_mode);

		break;

	case NOC_SWITCHING_PRODUCER_ACK:

		//Set CS enabled
		if (p->cs_mode){
			ctp_ptr = get_ctp_ptr(p->cs_net, DMNI_RECEIVE_OP);
			ctp_ptr->CS_enabled = 1;
			//puts("CS mode enabled at consumer\n"); //Deixar esse comentario, eh importante, removi para metricas de tempo
		} else {
			puts("PS mode enabled at consumer\n");
		}

		//Send confirmation to QoS master
		message[0] = NOC_SWITCHING_CTP_CONCLUDED;
		message[1] = p->producer_task;
		message[2] = p->consumer_task;
		message[3] = p->cs_mode;

		if (qos_master_addr == net_address){

			write_local_service_to_MA(qos_master_id, message, 4);

		} else {

			send_service_to_MA(qos_master_id, qos_master_addr, message, 4);

			while(HAL_is_send_active(PS_SUBNET));

		}

		break;
	}

}



