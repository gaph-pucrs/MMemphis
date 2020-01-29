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


/*Messages used to control the configuration packet*/
unsigned int config_message[SDN_XCLUSTER*SDN_XCLUSTER*4+13];//Worst case of config message (PE COUNT + (SIZE OF EACH MSG) + ACK MESSAGE)
unsigned char config_size;
unsigned int source_addr;
unsigned int target_addr;

/*Global offset initialized once into init_cluster_address_offset() of noc_manager.c. They properly access the matrix of CS routers status and keys*/
unsigned int x_offset, y_offset;


/** Initializes the two 2D arrays used to store the key of each DMNI. Each DMNI stores two keys and for this reason are necessary two arrays.
 * The size of the keys is 16 bits.
 *
 * */
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

void InitConfigRouter(){

	config_size = 0;
	source_addr = -1;
	target_addr = -1;
	//Necessary to avoid the data of config_message from previous packet (if exists) be overwriten
	while(!NoCSendFree());
}

void CommitConfigRouter(int requester_ID){

	unsigned int requester_proc;
	unsigned int config_packet_payload;
	const int const_packet_size = 13;//13 is the size of a standard Service Packet, see packet.h.
	const int ack_msg_data_size = 3; //3 is the size of the additional payload, handled by the MA task of requester

	config_packet_payload = config_size + const_packet_size + ack_msg_data_size;
	//Set the payload size of each sub-packet
	for (int payload_size_index=1; payload_size_index<config_size; payload_size_index=payload_size_index+4){
		//Set the payload at each position of the sub-packets
		config_message[payload_size_index] = config_packet_payload-2;
		config_packet_payload = config_packet_payload - 4;
	}

	requester_proc = GetTaskLocation(requester_ID);

	//Add the last packet: the ack to the source kernel of the path
	config_message[config_size] = requester_proc;
	config_message[config_size+1] = const_packet_size-2 + ack_msg_data_size; //CONSTANT_PACKT_SIZE -2
	config_message[config_size+2] = SET_CS_ROUTER_ACK_MANAGER; //source
	config_message[config_size+4] = requester_ID; //consumer_task
	config_message[config_size+5] = source_addr; //source_PE
	config_message[config_size+8] = ack_msg_data_size; //msg_lenght
	config_message[config_size+9] = target_addr; //consumer_processor

	config_size = config_size + const_packet_size; //Fills the remaining of the message

	config_message[config_size++] = SET_CS_ROUTER_ACK_MANAGER; //consumer_processor
	config_message[config_size++] = source_addr; //consumer_processor
	config_message[config_size++] = target_addr; //consumer_processor


	//Just print the message before sending it
	/*putsv("pkt_size = ", config_size);
	for (int k=0; k<config_size; k++){
		Puts("["); Puts(itoa(k)); Puts("]="); Puts(itoh(config_message[k])); Puts("\n");
	}*/

	SendRaw(config_message, config_size);

}

void ConfigRouter(unsigned int target, unsigned int input_port, unsigned int output_port, unsigned int subnet){

	unsigned int config_msg, x, y;
	unsigned short int k1, k2, k3;

	//5 is a value used by software to distinguish between the LOCAL_IN e LOCAL_OUT port
	//At the end of all, the port 5 not exists inside the router, and the value is configured to 4.
	if (input_port == 5)
		input_port = 4;
	else if (output_port == 5)
		output_port = 4;

	config_msg = (input_port << 13) | (output_port << 10) | (2 << subnet) | 0;

	//Extracts x and y
	x = target >> 8;
	y = target & 0xFF;

	//Remove offset
	x = x - x_offset;
	y = y - y_offset;

	//Gets the current key
	k1 = key1[x][y];
	k2 = key2[x][y];
	//Generate k3
	k3 = (unsigned short)(GetTick()); //random()

	//Puts("K3: "); Puts(itoh(k3)); Puts("\n");
	//Update keys
	key1[x][y] = k2;
	key2[x][y] = k3;

	//Config packet update
	config_message[config_size++] = 0x10000 | target;;
	config_size++; //Spacte used to t
	config_message[config_size++] = ((k1 ^ k2) << 16) | (k2 ^ k3);
	config_message[config_size++] = config_msg;

	if (target_addr == -1){
		target_addr = target;
	}

	//Always update source_addr, the last call will set the source since retrace execute backtrack
	source_addr = target;

}


#endif /* APPLICATIONS_MAN_APP_SDN_INCLUDES_DMNI_KEY_MANAGER_H_ */
