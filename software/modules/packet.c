/*!\file packet.c
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * This module implements function relative to programming the DMNI to send and receibe a packet.
 * \detailed
 * It is a abstraction from the NoC to the software components.
 * This module is used by both manager and slave kernel
 */

#include "packet.h"
#include "utils.h"
#include "../include/plasma.h"

volatile ServiceHeaderSlot sh_slot1, sh_slot2;	//!<Slots to prevent memory writing while is sending a packet

unsigned int global_inst = 0;			//!<Global CPU instructions counter

unsigned int net_address;				//!<Global net_address


/**Searches for a free ServiceHeaderSlot (sh_slot1 or sh_slot2) pointer.
 * A free slot is the one which is not being used by DMNI. This function prevents that
 * a given memory space be changed while its is not completely transmitted by DMNI.
 * \return A pointer to a free ServiceHeadeSlot
 */
ServiceHeader * get_service_header_slot() {

	if ( sh_slot1.status ) {

		sh_slot1.status = 0;
		sh_slot2.status = 1;
		return (ServiceHeader*) &sh_slot1.service_header;

	} else {

		sh_slot2.status = 0;
		sh_slot1.status = 1;
		return (ServiceHeader*) &sh_slot2.service_header;
	}
}

/**Initializes the service slots
 */
void init_packet(){
	sh_slot1.status = 1;
	sh_slot2.status = 1;

	net_address = MemoryRead(NI_CONFIG);
}

/**Function that say if the send process of DMNI is active
 * \param subnet Number of the required subnet
 * \return 1 - if active, 0 - if not active
 */
#define is_send_active(subnet) (MemoryRead(DMNI_SEND_ACTIVE) & (1 << subnet))

/**Function that say if the receive process of DMNI is active
 * \param subnet Number of the required subnet
 * \return 1 - if active, 0 - if not active
 */
/*int is_receive_active(unsigned int subnet){
	return ( MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet) );
}*/

/**Function that abstracts the DMNI programming for read data from NoC and copy to memory
 * \param initial_address Initial memory address to copy the received data
 * \param dmni_msg_size Data size, is represented in memory word of 32 bits
 * \param subnet_nr Number of the subnet to read the data
 */
void DMNI_read_data(unsigned int initial_address, unsigned int dmni_msg_size){

	MemoryWrite(DMNI_NET, PS_SUBNET);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, initial_address);
	MemoryWrite(DMNI_MEM_SIZE, dmni_msg_size);

	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << PS_SUBNET));
}


/**Function that abstracts the DMNI programming for read data from CS NoC and copy to memory
 * The function discoveries the data size at 1st DMMI programming, to copy the data at the 2nd programming
 * \param initial_address Initial memory address to copy the received data
 * \param subnet_nr Number of the subnet to read the data
 */
inline unsigned int DMNI_read_data_CS(unsigned int initial_address, unsigned int subnet_nr){

	volatile unsigned int size_32bits;

	//Discovery the data size
	MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, (unsigned int)&size_32bits);
	MemoryWrite(DMNI_MEM_SIZE, 1);

	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet_nr));
	//Copies to the memory
	MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, (unsigned int)initial_address);
	MemoryWrite(DMNI_MEM_SIZE, size_32bits);

	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet_nr));

	return size_32bits;
 }



/**Function that abstracts the DMNI programming for send data from memory to NoC
 * \param initial_address Initial memory address that will be transmitted to NoC
 * \param dmni_msg_size Data size, is represented in memory word of 32 bits
 * \param subnet_nr Number of the subnet to send the data
 */
void DMNI_send_data(unsigned int initial_address, unsigned int dmni_msg_size,  unsigned int subnet_nr){

	while ( MemoryRead(DMNI_SEND_ACTIVE) & (1 << subnet_nr) );

	MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_SEND_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, initial_address);
	MemoryWrite(DMNI_MEM_SIZE, dmni_msg_size);
}

/**Function that abstracts the process to send a generic packet to NoC by programming the DMNI
 * \param p Packet pointer
 * \param initial_address Initial memory address of the packet payload (payload, not service header)
 * \param dmni_msg_size Packet payload size represented in memory words of 32 bits
 */
void send_packet(ServiceHeader *p, unsigned int initial_address, unsigned int dmni_msg_size){

	p->payload_size = (CONSTANT_PKT_SIZE - 2) + dmni_msg_size;

	//p->cpu_slack_time = 0;

	p->source_PE = net_address;

	//Waits the DMNI send process be released
	while ( MemoryRead(DMNI_SEND_ACTIVE) & (1 << PS_SUBNET));

	p->timestamp = MemoryRead(TICK_COUNTER);

	MemoryWrite(DMNI_NET, PS_SUBNET);
	MemoryWrite(DMNI_OP, DMNI_SEND_OP);

	if (dmni_msg_size > 0){
		MemoryWrite(DMNI_MEM_ADDR2, initial_address);
		MemoryWrite(DMNI_MEM_SIZE2, dmni_msg_size);
	}

	MemoryWrite(DMNI_MEM_ADDR, (unsigned int) p);
	MemoryWrite(DMNI_MEM_SIZE, CONSTANT_PKT_SIZE);

}

/**Function that abstracts the process to read a generic packet from NoC by programming the DMNI
 * \param p Packet pointer
 */
void read_packet(ServiceHeader *p){

	MemoryWrite(DMNI_NET, PS_SUBNET); //default is ps subnoc
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, (unsigned int) p);
	MemoryWrite(DMNI_MEM_SIZE, CONSTANT_PKT_SIZE);

	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << PS_SUBNET));
}

/**Function that abstracts the process to configure the CS routers table,
 * This function configures the IRT and ORT CS_router tables
 * \param input_port Input port number [0-4]
 * \param output_port Output port number [0-4]
 * \param subnet_nr Subnet number
 */
void config_subnet(unsigned int input_port, unsigned int output_port, unsigned int subnet_nr){

#if CS_DEBUG
	puts("\n **** CONFIG subnet routine *****\n");
	putsv("Input port = ", input_port);
	putsv("Output port = ", output_port);
	putsv("subnet_nr = ", subnet_nr);
#endif
	volatile unsigned int config;

	config = (input_port << 13) | (output_port << 10) | (2 << subnet_nr) | 0;

	MemoryWrite(CONFIG_VALID_NET, config);
}


//COISAS DE DISTURBING, APAGAR DEPOIS


#define DISTURBING_DATA_SIZE	7100//8000//5096
#define DISTURBING_WAITING		1
#define DISTURBING_PAIRS		2

#define	noc_ps_interruption 	(MemoryRead(IRQ_STATUS) >= IRQ_INIT_NOC)//!< Signals a incoming packet from PS NoC

			//prod, cons
unsigned int disturbing_list[DISTURBING_PAIRS][2] = {{0x305, 0x005},{0x001, 0x304}};

//unsigned int disturbing_list[DISTURBING_PAIRS][2] = {{0x002, 0x200},{0x001, 0x301}};

//unsigned int disturbing_list[DISTURBING_PAIRS][2] = {{0x002, 0x300}};

unsigned int targetPE;

int is_disturbing_prod_PE(){

#if DISABLE_DISTURBING
	return 0;
#endif

	for(int i=0; i<DISTURBING_PAIRS; i++){
		if (net_address == disturbing_list[i][0]){
			puts("\nTHIS PE IS A DISTURBING PRODUCING PE\nTarget = ");
			targetPE = disturbing_list[i][1];
			puts(itoh(targetPE)); puts("\n");
			return 1;
		}
	}
	return 0;
}

int is_disturbing_cons_PE(){

#if DISABLE_DISTURBING
	return 0;
#endif

	for(int i=0; i<DISTURBING_PAIRS; i++){
		if (net_address == disturbing_list[i][1]){
			puts("\nTHIS PE IS A DISTURBING CONSUMER PE\n");
			return 1;
		}
	}
	return 0;
}


void disturbing_generator(){

	volatile int i;
	volatile unsigned int data[3];
	int start_time, finish_time;

	puts("Init disturbing generator\n");

	data[0] = targetPE;

	data[1] = DISTURBING_DATA_SIZE-2;

	data[2] = 0x220;

	puts(itoh(data[1])); puts("\n");


	/*if (net_address == 0x305){
		start_time = 7;
		finish_time = 500000;
	} else if (net_address == 0x304){
		start_time = 7;
		finish_time = 500000;
	}*/

	start_time = 1;
	finish_time = 500000;

	/*if (net_address == 0x002){
		start_time = 66; //50
		finish_time = 90;
	} else if (net_address == 0x001){
		start_time = 25;
		finish_time = 40;
	}*/


	while(MemoryRead(TICK_COUNTER) < start_time*100000);
	while(MemoryRead(TICK_COUNTER) < finish_time*100000){
	//while(1){

		puts("Produziu\n");
		DMNI_send_data((unsigned int)&data, DISTURBING_DATA_SIZE, PS_SUBNET);

		for(i=0; i<DISTURBING_WAITING; i++);
	}

}

void disturbing_reader(){

	puts("Init disturbing consumer\n");

	while(1){

		if (noc_ps_interruption){
			DMNI_read_data((PAGE_SIZE*2)-4, DISTURBING_DATA_SIZE);
			puts("Received\n");
		}
	}
}
