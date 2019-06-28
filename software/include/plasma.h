/*!\file plasma.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Edited by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief  Plasma Hardware Definitions
 */

#ifndef __PLASMA_H__
#define __PLASMA_H__

#define ENABLE_LATENCY_MISS		1
#define ENABLE_DEADLINE_MISS	1
#define ENABLE_APL_SLAVE		1
#define ENABLE_QOS_ADAPTATION	1
#define ENABLE_QOS_PREDICTION	1

#define DISABLE_DISTURBING		0

#define APL_WINDOW	1000000
//#define APL_WINDOW	800000




/*********** Hardware addresses ***********/
#define UART_WRITE        	0x20000000
#define UART_READ         	0x20000000
#define IRQ_MASK          	0x20000010
#define IRQ_STATUS        	0x20000020
#define TIME_SLICE       	0x20000060
#define SYS_CALL		   	0x20000070
#define END_SIM 		   	0x20000080
#define CLOCK_HOLD 		   	0x20000090

#define	NI_CONFIG			0x20000140

//Router Config
#define	 EAST	0
#define	 WEST	1
#define	 NORTH	2
#define	 SOUTH	3
#define	 LOCAL	4
#define  UNUSED 5

#define CONFIG_VALID_NET	 	0x20000150
#define CONFIG_CLEAR_NET 		0x20000160

//DMNI Config
#define DMNI_NET				0x20000200
#define DMNI_MEM_ADDR			0x20000210
#define DMNI_MEM_SIZE			0x20000214
#define DMNI_MEM_ADDR2			0x20000220
#define DMNI_MEM_SIZE2			0x20000224
#define DMNI_OP					0x20000230

#define DMNI_SEND_ACTIVE	  	0x20000250
#define DMNI_RECEIVE_ACTIVE		0x20000260

#define DMNI_SEND_OP	  		0
#define DMNI_RECEIVE_OP			1


//Scheduling report
#define SCHEDULING_REPORT		0x20000270
#define INTERRUPTION		0x10000
//#define SYSCALL			0x20000
#define SCHEDULER			0x40000
#define IDLE				0x80000

//Communication graphical debbug
#define ADD_PIPE_DEBUG			0x20000280
#define REM_PIPE_DEBUG			0x20000285
#define ADD_REQUEST_DEBUG		0x20000290
#define REM_REQUEST_DEBUG		0x20000295

/* Kernel Status */
#define PE_STATUS_MIGRATING		0
#define PE_STATUS_ALLOCATING	1
#define PE_STATUS_RUNNING		2


/* DMNI operations */
#define READ	0
#define WRITE	1

#define TICK_COUNTER	  		0x20000300

//Circuit-Switching Request
#define WRITE_CS_REQUEST		0x20000310
#define READ_CS_REQUEST			0x20000320
#define HANDLE_CS_REQUEST		0x20000330

#define REQ_APP		  			0x20000350
#define ACK_APP		  			0x20000360

#define SLACK_TIME_MONITOR		0x20000370

//Kernel pending service FIFO
#define PENDING_SERVICE_INTR	0x20000400

#define SLACK_TIME_WINDOW		100000 // half milisecond

/*********** Interrupt bits **************/
#define IRQ_SCHEDULER				0x01 //bit 0
#define IRQ_PENDING_SERVICE			0x02 //bit 1
#define IRQ_SLACK_TIME				0x04 //bit 2
#define IRQ_CS_REQUEST				0x08 //bit 3
#define IRQ_INIT_NOC				0x10 //bit 4
         
/*Memory Access*/
#define MemoryRead(A) (*(volatile unsigned int*)(A))
#define MemoryWrite(A,V) *(volatile unsigned int*)(A)=(V)

#endif /*__PLASMA_H__*/

