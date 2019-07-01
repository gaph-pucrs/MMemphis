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
#include "../../../include/kernel_pkg.h"
#include "../../include/plasma.h"
#include "utils.h"

#define MAX_CTP	(SUBNETS_NUMBER-1) * 2 //!< Maximum number of ctp into a slave processor

CTP ctp[ MAX_CTP ];

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
