/*
 * HAL.h
 *
 *  Created on: Aug 7, 2019
 *      Author: ruaro
 *
 *  HAL implement the abstraction between the software and hardware. The ideai is to change only this file
 *  when the kernel is ported to another CPU or archicteture
 */

#ifndef SOFTWARE_HAL_H_
#define SOFTWARE_HAL_H_

#include "../../../include/kernel_pkg.h"
//HAL_model_properties.h"

/*Onique global variable used by more then 1 module, holds the address XY of the current PE*/
extern unsigned int net_address;

/*********** Hardware MMR addresses ***********/
#define UART_WRITE        		0x20000000
#define IRQ_MASK          		0x20000010
//#define IRQ_STATUS        		0x20000020 - Only used in assembly
#define TIME_SLICE       		0x20000060
#define SYS_CALL		   		0x20000070
#define END_SIM 		   		0x20000080
#define CLOCK_HOLD 		   		0x20000090
#define	NET_ADDRESS				0x20000140
#define CONFIG_VALID_NET	 	0x20000150
#define CONFIG_CLEAR_NET 		0x20000160
//DMNI Config
#define DMNI_NET				0x20000200
#define DMNI_MEM_ADDR			0x20000210
#define DMNI_MEM_SIZE			0x20000214
#define DMNI_MEM_ADDR2			0x20000220
#define DMNI_MEM_SIZE2			0x20000224
#define DMNI_OP					0x20000230
#define DMNI_SEND_STATUS	  	0x20000250
#define DMNI_RECEIVE_STATUS		0x20000260
#define SCHEDULING_REPORT		0x20000270
#define ADD_PIPE_DEBUG			0x20000280
#define REM_PIPE_DEBUG			0x20000285
#define ADD_REQUEST_DEBUG		0x20000290
#define REM_REQUEST_DEBUG		0x20000295
#define TICK_COUNTER	  		0x20000300
/* Circuit-Switching Request MMR addresses */
#define WRITE_CS_REQUEST		0x20000310
#define READ_CS_REQUEST			0x20000320
#define HANDLE_CS_REQUEST		0x20000330
/* Slack Timer Monitor MMR addresses */
#define SLACK_TIME_MONITOR		0x20000370
/* Kernel pending service FIFO */
#define PENDING_SERVICE_INTR	0x20000400
/* Debugging MMR addresses */
#define INTERRUPTION			0x10000
#define SCHEDULER				0x40000
#define IDLE					0x80000

//Router Config
#define	 EAST	0
#define	 WEST	1
#define	 NORTH	2
#define	 SOUTH	3
#define	 LOCAL	4
#define  UNUSED 5

/*Used to filter the IRQ status with only the value of PS interruption*/
#define IRQ_PS					(IRQ_INIT_NOC << (SUBNETS_NUMBER-1) )

/*Used by DAPE to define the application window period */
#define APL_WINDOW				1000000

/* Kernel Status */
#define PE_STATUS_MIGRATING		0
#define PE_STATUS_ALLOCATING	1
#define PE_STATUS_RUNNING		2

/*Sets the slack time windows period in clock cycles*/
#define SLACK_TIME_WINDOW		100000 // half milisecond - Remove from here because here is the place of thing related to hardware

/* DMNI hardware working mode */
#define DMNI_SEND_OP	  		0
#define DMNI_RECEIVE_OP			1


/* Defines the number where Packet-Switching subnet is refered in MMR*/
#define PS_SUBNET 				(SUBNETS_NUMBER-1)

/*********** IRQ Interrupt bits **************/
#define IRQ_SCHEDULER			0x01 //bit 0
#define IRQ_PENDING_SERVICE		0x02 //bit 1
#define IRQ_SLACK_TIME			0x04 //bit 2
#define IRQ_CS_REQUEST			0x08 //bit 3
#define IRQ_INIT_NOC			0x10 //bit 4

/*MMR read functions*/
#define HAL_get_tick() 					(*(volatile unsigned int*)(TICK_COUNTER))
#define HAL_get_irq_status() 			(*(volatile unsigned int*)(IRQ_MASK))
#define	HAL_get_core_addr()				(*(volatile unsigned int*)(NET_ADDRESS))
#define HAL_is_send_active(subnet) 		((*(volatile unsigned int*)(DMNI_SEND_STATUS)) & (1 << subnet))
#define HAL_is_receive_active(subnet) 	((*(volatile unsigned int*)(DMNI_RECEIVE_STATUS)) & (1 << subnet))
#define HAL_get_CS_request()			(*(volatile unsigned int*)(READ_CS_REQUEST))

/*MMR write functions*/
#define HAL_set_irq_mask(mask)			*(volatile unsigned int*)(IRQ_MASK)=(mask)
#define HAL_set_uart_str(str)			*(volatile unsigned int*)(UART_WRITE)=(str)
#define HAL_set_dmni_net(net)			*(volatile unsigned int*)(DMNI_NET)=(net)
#define HAL_set_dmni_op(op)				*(volatile unsigned int*)(DMNI_OP)=(op)
#define HAL_set_dmni_mem_addr(addr)		*(volatile unsigned int*)(DMNI_MEM_ADDR)=(addr)
#define HAL_set_dmni_mem_size(size)		*(volatile unsigned int*)(DMNI_MEM_SIZE)=(size)
#define HAL_set_dmni_mem_addr2(addr)	*(volatile unsigned int*)(DMNI_MEM_ADDR2)=(addr)
#define HAL_set_dmni_mem_size2(size)	*(volatile unsigned int*)(DMNI_MEM_SIZE2)=(size)
#define HAL_set_CS_config(config)		*(volatile unsigned int*)(CONFIG_VALID_NET)=(config)
#define HAL_set_clock_hold(on_off)		*(volatile unsigned int*)(CLOCK_HOLD)=(on_off)
#define HAL_set_pending_service(srv)	*(volatile unsigned int*)(PENDING_SERVICE_INTR)=(srv)
#define HAL_set_CS_request(subnet)		*(volatile unsigned int*)(WRITE_CS_REQUEST)=(subnet)
#define HAL_set_slack_time_monitor(t)	*(volatile unsigned int*)(SLACK_TIME_MONITOR)=(t)
#define HAL_set_scheduling_report(code)	*(volatile unsigned int*)(SCHEDULING_REPORT)=(code)
#define HAL_set_time_slice(t_slice)		*(volatile unsigned int*)(TIME_SLICE)=(t_slice)
#define HAL_handle_CS_request(req_st)	*(volatile unsigned int*)(HANDLE_CS_REQUEST)=(req_st)

/*** Externs of the HAL function implemented in assembly (file HAL_kernel_asm.S) ***/
extern void HAL_run_scheduled_task(unsigned int);
extern void HAL_set_interrupt_enabled(unsigned int);
/****************************************************************************/

/*Initializing function, for now, only set the value of net_address*/
void init_HAL();

/*IRQ abstraction*/
unsigned int HAL_interrupt_mask_clear(unsigned int);

unsigned int HAL_interrupt_mask_set(unsigned int);

/*DMNI Abstraction*/
inline unsigned int DMNI_read_data_CS(unsigned int, unsigned int);

void DMNI_read_data(unsigned int, unsigned int);

void DMNI_send_data(unsigned int, unsigned int, unsigned int);

void config_subnet(unsigned int, unsigned int, unsigned int);

/*UART abstraction*/
int puts(char *);

#define putsv(string, value) puts(string); puts(itoa(value)); puts("\n");


#endif /* SOFTWARE_HAL_H_ */
