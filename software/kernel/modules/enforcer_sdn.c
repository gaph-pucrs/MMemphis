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
unsigned int	ctp_producer_adress;	//!< Used to set a CTP online

void init_ctp(){
	for(int i=0; i<MAX_CTP; i++){
		ctp[i].subnet = -1;
		ctp[i].dmni_op = -1;
		ctp[i].ready_to_go = 0;
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
			ctp[i].ready_to_go = 0;

//#if CS_DEBUG
			puts("\n-----\nAdded ctp pair   "); puts(itoa(producer_task)); puts(" -> "); puts(itoa(consumer_task));
			puts(" to subnet "); puts(itoa(subnet)); putsv(" dmni op ", dmni_op);
//#endif
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

//#if CS_DEBUG
			puts("\n------\nRemoved ctp pair   "); puts(itoa(ctp[i].producer_task)); puts(" -> "); puts(itoa(ctp[i].consumer_task));
			putsv(" to subnet ", subnet);
//#endif
			ctp[i].subnet = -1;
			ctp[i].dmni_op = -1;
			ctp[i].ready_to_go = 0;
			return;
		}
	}

	puts("ERROR - no CTP to remove\n");
	while(1);
}


int get_subnet(int producer_task, int consumer_task, int dmni_op){
	for(int i=0; i<MAX_CTP; i++){
		if (ctp[i].producer_task == producer_task && ctp[i].consumer_task == consumer_task && ctp[i].dmni_op == dmni_op){
			if (ctp[i].ready_to_go == 0)
				return -1;
			//putsv("Return subnet ", ctp[i].subnet);
			return ctp[i].subnet;
		}
	}
	return -1;
}


void set_ctp_producer_adress(unsigned int ctp_addr){
	ctp_producer_adress = ctp_addr;
}

/**Add the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This procedure ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_add TCB pointer of the task to allocate the CTP
 */
void add_ctp_online(TCB * task_to_add){

	ServiceHeader * p;
	int producer_task, subnet;

	producer_task = task_to_add->add_ctp & 0xFFFF;
	subnet = task_to_add->add_ctp >> 16;

	add_ctp(producer_task, task_to_add->id, DMNI_RECEIVE_OP, subnet);

	p = get_service_header_slot();

	//p->header = get_task_location(producer_task);
	p->header = ctp_producer_adress;

	p->service = SET_NOC_SWITCHING_PRODUCER;

	p->producer_task = producer_task;

	p->consumer_task = task_to_add->id;

	p->cs_mode = 1;

	p->cs_net = subnet;

	send_packet(p, 0, 0);

	while(HAL_is_send_active(PS_SUBNET));

	task_to_add->add_ctp = 0;
}

/**Remove the CTP instance from the CTP array structure, redirect the order to remove the CTP to
 * the producer task. This producere ensure that no message is lost, because the consumer changes CS
 * before the producer.
 * \param task_to_remove TCB pointer of the task to remove the CS
 */
void remove_ctp_online(TCB * task_to_remove){

	ServiceHeader * p;

	//puts("remove_ctp_online\n");

	int subnet = get_subnet(task_to_remove->remove_ctp, task_to_remove->id, DMNI_RECEIVE_OP);

	if (subnet == -1){
		puts("ERROR, subnet -1\n");
		while(1);
	}

	remove_ctp(subnet, DMNI_RECEIVE_OP);

	p = get_service_header_slot();

	//p->header = get_task_location(task_to_remove->remove_ctp);
	p->header = ctp_producer_adress;

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

void send_config_router(int target_proc, int input_port, int output_port, int cs_net){

  volatile unsigned int config_pkt[3];

	//5 is a value used by software to distinguish between the LOCAL_IN e LOCAL_OUT port
	//At the end of all, the port 5 not exists inside the router, and the value is configured to 4.
	if (input_port == 5)
		input_port = 4;
	else if (output_port == 5)
		output_port = 4;

	if (target_proc == net_address){

		config_subnet(input_port, output_port, cs_net);

	} else {

		config_pkt[0] = 0x10000 | target_proc;

		config_pkt[1] = 1;

		config_pkt[2] = (input_port << 13) | (output_port << 10) | (2 << cs_net) | 0;

		DMNI_send_data((unsigned int)config_pkt, 3 , PS_SUBNET);

		while (HAL_is_receive_active((1 << PS_SUBNET)));

	}
#if CS_DEBUG
	puts("\n********* Send config router ********* \n");
	puts("target_proc = "); puts(itoh(target_proc));
	puts("\ninput port = "); puts(itoa(input_port));
	puts("\noutput_port = "); puts(itoa(output_port));
	puts("\ncs_net = "); puts(itoa(cs_net)); puts("\n***********************\n");
#endif
}

/** Assembles and sends a CTP_CS_ACK packet to the master kernel, signalizing
 * that the CS release or stablisment was accomplished
 *  \param producer_task Task ID of the CTP's producer task
 *  \param consumert_task Task ID of the CTP's consumer task
 *  \param subnet Subnet ID
 *  \param cs_mode Flag indicating to release or stablish a CS connection (1 - stablish, 0 - release)
 */
void send_NoC_switching_ack(int producer_task, int consumert_task, int subnet, int cs_mode){

	ServiceHeader * p;
	TCB * prod_tcb_ptr;

	prod_tcb_ptr = searchTCB(producer_task);

	if (!prod_tcb_ptr){
		puts("ERROR, not TCB found in send_cs_ack\n");
		for(;;);
	}

	p = get_service_header_slot();

	p->header = prod_tcb_ptr->master_address;

	p->service = NOC_SWITCHING_PRODUCER_ACK;

	p->producer_task = producer_task;

	p->consumer_task = consumert_task;

	p->cs_net = subnet;

	p->cs_mode = cs_mode;

	send_packet(p, 0, 0);


	//Sends the ACK to the consumer also
	if (cs_mode) {//Only send if the mode is establish

		//puts("Enviou NOC_SWITCHING_PRODUCER_ACK para consumer\n");
		p = get_service_header_slot();

		p->header = get_task_location(consumert_task);

		p->service = NOC_SWITCHING_PRODUCER_ACK;

		p->producer_task = producer_task;

		p->consumer_task = consumert_task;

		p->cs_net = subnet;

		p->cs_mode = cs_mode;

		send_packet(p, 0, 0);
	}

	//puts("send ACK to manager\n");
}


void handle_dynamic_CS_setup(volatile ServiceHeader * p){

	TCB * tcb_ptr = 0;
	CTP * ctp_ptr = 0;

	switch (p->service) {

	case SET_NOC_SWITCHING_CONSUMER:

		tcb_ptr = searchTCB(p->consumer_task);

		if (p->cs_mode)  //Stablish Circuit-Switching

			tcb_ptr->add_ctp = p->cs_net << 16 | p->producer_task;

		else  //Stablish Packet-Switching

			tcb_ptr->remove_ctp = p->producer_task;


		set_ctp_producer_adress(p->allocated_processor);

		//Test if the task is not waiting for a message
		if (!tcb_ptr->scheduling_ptr->waiting_msg){

			check_ctp_reconfiguration(tcb_ptr);
		}

		break;

	case SET_NOC_SWITCHING_PRODUCER:

		puts("++++++ CTP_CLEAR_TO_PRODUCER\n");

		if (p->cs_mode) {
			ctp_ptr = add_ctp(p->producer_task, p->consumer_task, DMNI_SEND_OP, p->cs_net);
		} else
			remove_ctp(p->cs_net, DMNI_SEND_OP);


		send_NoC_switching_ack(p->producer_task, p->consumer_task, p->cs_net, p->cs_mode);

		if (p->cs_mode){
			//puts("Ready to go at producer\n");
			ctp_ptr->ready_to_go = 1;
		}

		break;

	case NOC_SWITCHING_PRODUCER_ACK:

		ctp_ptr = get_ctp_ptr(p->cs_net, DMNI_RECEIVE_OP);

		ctp_ptr->ready_to_go = 1;

		//puts("Ready to go at consumer\n");
		//puts("READY TO GO RECEIVED, producer "); puts(itoa(ctp_ptr->producer_task)); putsv(" consumer ", ctp_ptr->consumer_task);

		break;
	}

}



