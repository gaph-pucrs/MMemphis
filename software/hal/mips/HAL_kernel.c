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

#include "HAL_kernel.h"

unsigned int global_inst = 0;			//!<Global CPU instructions counter
unsigned int net_address;				//!<Global net_address


/**Function that say if the receive process of DMNI is active
 * \param subnet Number of the required subnet
 * \return 1 - if active, 0 - if not active
 */
/*int is_receive_active(unsigned int subnet){
	return ( MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet) );
}*/


void init_HAL(){
	net_address = HAL_get_core_addr();
}

/**Function that abstracts the DMNI programming for read data from NoC and copy to memory
 * \param initial_address Initial memory address to copy the received data
 * \param dmni_msg_size Data size, is represented in memory word of 32 bits
 * \param subnet_nr Number of the subnet to read the data
 */
void DMNI_read_data(unsigned int initial_address, unsigned int dmni_msg_size){

	HAL_set_dmni_net(PS_SUBNET);
	HAL_set_dmni_op(DMNI_RECEIVE_OP);
	HAL_set_dmni_mem_addr(initial_address);
	HAL_set_dmni_mem_size(dmni_msg_size);

	/*MemoryWrite(DMNI_NET, PS_SUBNET);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP);
	MemoryWrite(DMNI_MEM_ADDR, initial_address);
	MemoryWrite(DMNI_MEM_SIZE, dmni_msg_size);
	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << PS_SUBNET));*/

	while(HAL_is_receive_active(PS_SUBNET));
}


/**Function that abstracts the DMNI programming for read data from CS NoC and copy to memory
 * The function discoveries the data size at 1st DMMI programming, to copy the data at the 2nd programming
 * \param initial_address Initial memory address to copy the received data
 * \param subnet_nr Number of the subnet to read the data
 */
inline unsigned int DMNI_read_data_CS(unsigned int initial_address, unsigned int subnet_nr){

	volatile unsigned int size_32bits;

	//Discovery the data size
	/*MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, (unsigned int)&size_32bits);
	MemoryWrite(DMNI_MEM_SIZE, 1);*/

	HAL_set_dmni_net(PS_SUBNET);
	HAL_set_dmni_op(DMNI_RECEIVE_OP);
	HAL_set_dmni_mem_addr((unsigned int)&size_32bits);
	HAL_set_dmni_mem_size(1);

	/*while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet_nr));*/

	while(HAL_is_receive_active(subnet_nr));

	//Copies to the memory
	/*MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_RECEIVE_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, (unsigned int)initial_address);
	MemoryWrite(DMNI_MEM_SIZE, size_32bits);
	while (MemoryRead(DMNI_RECEIVE_ACTIVE) & (1 << subnet_nr));*/

	HAL_set_dmni_net(subnet_nr);
	HAL_set_dmni_op(DMNI_RECEIVE_OP); // send is 0, receive is 1
	HAL_set_dmni_mem_addr((unsigned int)initial_address);
	HAL_set_dmni_mem_size(size_32bits);

	while(HAL_is_receive_active(subnet_nr));

	return size_32bits;
 }



/**Function that abstracts the DMNI programming for send data from memory to NoC
 * \param initial_address Initial memory address that will be transmitted to NoC
 * \param dmni_msg_size Data size, is represented in memory word of 32 bits
 * \param subnet_nr Number of the subnet to send the data
 */
void DMNI_send_data(unsigned int initial_address, unsigned int dmni_msg_size,  unsigned int subnet_nr){

	//while ( MemoryRead(DMNI_SEND_STATUS) & (1 << subnet_nr) );
	while (HAL_is_send_active(subnet_nr));

	/*MemoryWrite(DMNI_NET, subnet_nr);
	MemoryWrite(DMNI_OP, DMNI_SEND_OP); // send is 0, receive is 1
	MemoryWrite(DMNI_MEM_ADDR, initial_address);
	MemoryWrite(DMNI_MEM_SIZE, dmni_msg_size);*/

	HAL_set_dmni_net(subnet_nr);
	HAL_set_dmni_op(DMNI_SEND_OP); // send is 0, receive is 1
	HAL_set_dmni_mem_addr(initial_address);
	HAL_set_dmni_mem_size(dmni_msg_size);
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

	HAL_set_CS_config(config);
}


/** Clear a interruption mask
 * \param Mask Interruption mask clear
 */
unsigned int HAL_interrupt_mask_clear(unsigned int Mask) {

    unsigned int mask;

    mask = HAL_get_irq_status() & ~Mask;
    HAL_set_irq_mask(mask);

    return mask;
}

/** Set a interruption mask
 * \param Mask Interruption mask set
 */
unsigned int HAL_interrupt_mask_set(unsigned int Mask) {

    unsigned int mask;

    mask = HAL_get_irq_status() | Mask;
    HAL_set_irq_mask(mask);

    return mask;
}

/**Print the string in the text file log
 * \param string array of chars
 * \return The int return is only to avoid a build-in warning
 */
int puts(char *string) {

	int *str_part;
	//This is the most crazy and complicated FOR declaration that I ever seen. For obviously purposes, I divided the FOR section in lines
	//PS: This indicates a hardware developer putting his hands on software development
	for(
			str_part = (int*)string,  HAL_set_uart_str(*str_part);

			!( ( (char*)str_part )[0] == 0 || ( (char*)str_part )[1] == 0 || ( (char*)str_part )[2] == 0 || ( (char*)str_part )[3] == 0);

			*str_part++, HAL_set_uart_str(*str_part)
	);
	return 0;
}


