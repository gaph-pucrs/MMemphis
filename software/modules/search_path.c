/*
 * path_search.c
 *
 *  Created on: 30/08/2016
 *      Author: mruaro
 */

#include "search_path.h"
#include "utils.h"
#include "packet.h"
#include "../include/plasma.h"
#include "../include/services.h"

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

		while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << PS_SUBNET));
	}
#if CS_DEBUG
	puts("\n********* Send config router ********* \n");
	puts("target_proc = "); puts(itoh(target_proc));
	puts("\ninput port = "); puts(itoa(input_port));
	puts("\noutput_port = "); puts(itoa(output_port));
	puts("\ncs_net = "); puts(itoa(cs_net)); puts("\n***********************\n");
#endif
}
