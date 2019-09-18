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
#include "ctp.h"
#include "../../hal/mips/HAL_kernel.h"
#include "../../include/services.h"
#include "packet.h"
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

