/*
 * secure_sdn.h
 *
 *  Created on: 24 de out de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_SDN_INCLUDES_SECURE_SDN_H_
#define APPLICATIONS_MAN_APP_SDN_INCLUDES_SECURE_SDN_H_

#include "../common_include.h"

/*Secure SDN: array that stores the address of trusty manager of the system*/
unsigned int 	secure_mappers_addr[MAX_MAPPING_TASKS];


void initialize_secure_mappers(unsigned int * mapppers_addr){

	unsigned int task_id, proc_addr;

	for (int i=0; i<MAX_MAPPING_TASKS; i++){
		secure_mappers_addr[i] = mapppers_addr[i];
		task_id = secure_mappers_addr[i] >> 16;
		proc_addr = secure_mappers_addr[i] & 0xFFFF;
		Puts("Secure mapper MA "); Puts(itoa(task_id)); Puts(" allocated at "); Puts(itoh(proc_addr)); Puts("\n");
	}

}

int check_path_requester_authenticity(unsigned int id, unsigned int address){
	unsigned int task_id, proc_addr;

	for (int i=0; i<MAX_MAPPING_TASKS; i++){
		task_id = secure_mappers_addr[i] >> 16;
		proc_addr = secure_mappers_addr[i] & 0xFFFF;

		if (task_id == id && proc_addr == address){
			//Puts("Manager authorized to have access SDN services\n");
			return 1;
		}
	}

	//Puts("ID "); Puts(itoa(id)); Puts("\n");
	Puts("ATTENTION: Manager not authorized to have access SDN services "); Puts(itoh(address)); Puts("\n");
	return 0;
}



#endif /* APPLICATIONS_MAN_APP_SDN_INCLUDES_SECURE_SDN_H_ */
