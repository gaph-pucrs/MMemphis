/*
 * dmni_key_manager.h
 *
 *  Created on: 4 de nov de 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_SDN_INCLUDES_DMNI_KEY_MANAGER_H_
#define APPLICATIONS_MAN_APP_SDN_INCLUDES_DMNI_KEY_MANAGER_H_


#include "../common_include.h"


unsigned short int key1[SDN_XCLUSTER][SDN_YCLUSTER];
unsigned short int key2[SDN_XCLUSTER][SDN_YCLUSTER];


//K1
void initialize_keys(unsigned short int initial_k1, unsigned short int initial_k2){

	Puts("Init K1 = "); Puts(itoh(initial_k1)); Puts("\n");
	Puts("Init K2 = "); Puts(itoh(initial_k2)); Puts("\n");

	for(int x=0; x<SDN_XCLUSTER; x++){
		for(int y=0; y<SDN_YCLUSTER; y++){
			key1[x][y] = initial_k1;
			key2[x][y] = initial_k2;
		}
	}
}


void ConfigRouter(unsigned int target, unsigned int input_port, unsigned int output_port, unsigned int subnet){

	unsigned int config_msg, x, y;
	unsigned short int k1, k2, k3;
	unsigned int * message;

	//5 is a value used by software to distinguish between the LOCAL_IN e LOCAL_OUT port
	//At the end of all, the port 5 not exists inside the router, and the value is configured to 4.
	if (input_port == 5)
		input_port = 4;
	else if (output_port == 5)
		output_port = 4;

	config_msg = (input_port << 13) | (output_port << 10) | (2 << subnet) | 0;

	if (target == net_address){

		LocalSDNConfig(config_msg);

	} else { //Remote configuration

		x = target >> 8;
		y = target & 0xFF;

		//Gets the current key
		k1 = key1[x][y];
		k2 = key2[x][y];
		//Generate k3
		k3 = (unsigned short)(GetTick()); //random()

		Puts("K3: "); Puts(itoh(k3)); Puts("\n");
		//Update keys
		key1[x][y] = k2;
		key2[x][y] = k3;

		//Send cfg message
		message = get_message_slot();
		//Header
		message[0] = 0x10000 | target;
		//Payload
		message[1] = 2;
		//Key flit
		message[2] = ((k1 ^ k2) << 16) | (k2 ^ k3);
		//Msg flit
		message[3] = config_msg;

		/*message[2] = (k1 ^ k2);
		message[3] = config_msg;
		message[4] = (k2 ^ k3);*/

		SendRaw(message, 4);

	}

#if 0
	puts("\n********* Config router ********* \n");
	puts("target_proc = "); puts(itoh(target_proc));
	puts("\ninput port = "); puts(itoa(input_port));
	puts("\noutput_port = "); puts(itoa(output_port));
	puts("\ncs_net = "); puts(itoa(cs_net)); puts("\n***********************\n");
#endif
}


#endif /* APPLICATIONS_MAN_APP_SDN_INCLUDES_DMNI_KEY_MANAGER_H_ */
