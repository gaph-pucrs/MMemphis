/*
 * circuit_switching.h
 *
 *  Created on: 8 de nov de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_
#define APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_

#include "application.h"

int circuit_switching_active = 0;

int request_connection(Application * app){
 //1. Organize the CTPs
 //2. Send they to the SDN controller
 //3. Set app waiting reclustering

	return 1;
}

int is_CS_not_active(){
	return !circuit_switching_active;
}

void handle_connection_response(){


	//WHen the app no have more CTP set it as READY_TO_LOAD
}


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_LOCAL_MAPPER_CIRCUIT_SWITCHING_H_ */
