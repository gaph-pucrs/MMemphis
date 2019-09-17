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
#include "../../hal/mips/HAL_kernel.h"

volatile ServiceHeaderSlot sh_slot1, sh_slot2;	//!<Slots to prevent memory writing while is sending a packet


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
	//while ( MemoryRead(DMNI_SEND_ACTIVE) & (1 << PS_SUBNET)){
	while ( HAL_is_send_active(PS_SUBNET) );

	p->timestamp = HAL_get_tick();

	//MemoryWrite(DMNI_NET, PS_SUBNET);
	//MemoryWrite(DMNI_OP, DMNI_SEND_OP);
	HAL_set_dmni_net(PS_SUBNET);
	HAL_set_dmni_op(DMNI_SEND_OP);

	if (dmni_msg_size > 0){
		//MemoryWrite(DMNI_MEM_ADDR2, initial_address);
		//MemoryWrite(DMNI_MEM_SIZE2, dmni_msg_size);

		HAL_set_dmni_mem_addr2(initial_address);
		HAL_set_dmni_mem_size2(dmni_msg_size);
	}

	//MemoryWrite(DMNI_MEM_ADDR, (unsigned int) p);
	//MemoryWrite(DMNI_MEM_SIZE, CONSTANT_PKT_SIZE);

	HAL_set_dmni_mem_addr((unsigned int) p);
	HAL_set_dmni_mem_size(CONSTANT_PKT_SIZE);

}

/**Function that abstracts the process to read a generic packet from NoC by programming the DMNI
 * \param p Packet pointer
 */
void read_packet(ServiceHeader *p){

	DMNI_read_data((unsigned int) p, CONSTANT_PKT_SIZE);
}

