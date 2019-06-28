/*!\file kernel_master.h
 * HEMPS VERSION - 8.0 - support for RT applications
 *
 * Distribution:  June 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief
 * Header of kernel_master with important defines
 */

#ifndef __KERNEL_MASTER_H__
#define __KERNEL_MASTER_H__

#include "../../../include/kernel_pkg.h"
#include "../../include/plasma.h"

#define TASK_DESCRIPTOR_SIZE	16	//!< Size of the task descriptor into repository.txt file

/* Useful macros */
#define	noc_ps_interruption 	(MemoryRead(IRQ_STATUS) >= IRQ_INIT_NOC)//!< Signals a incoming packet from PS NoC
#define	app_req_reg 			(MemoryRead(REQ_APP) & 0x80000000)	//!< Signals a new application request from repository
#define	external_app_reg 		(MemoryRead(REQ_APP) & 0x7fffffff)	//!< Used to creates the repository reading address
//#define qos_monitorin_irq		(MemoryRead(IRQ_STATUS) & IRQ_SLACK_TIME)//!< Signals to starts a new monitoring round

//These functions are externed only for remove warnings into kernel_master.c code
void handle_new_app(int, volatile unsigned int *, unsigned int);

void initialize_slaves();

#endif
