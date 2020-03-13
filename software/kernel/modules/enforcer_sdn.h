/*!\file ctp.h
 * HEMPS VERSION - 8.0 - QoS
 *
 * Distribution:  August 2016
 *
 * Created by: Marcelo Ruaro - contact: marcelo.ruaro@acad.pucrs.br
 *
 * Research group: GAPH-PUCRS   -  contact:  fernando.moraes@pucrs.br
 *
 * \brief Store the informations for each CTP (Communication Task Pair).
 *
 * \detailed
 * A ctp is used in CS mode. When a task belowing to a ctp need to send or receive a packet, the kernel
 * access this structure to be aware about the other task of the ctp pair
 */

#ifndef CTP_H_
#define CTP_H_

#include "TCB.h"
#include "packet.h"

/**
 * \brief This structure store CTP information used in CS communication
 */
typedef struct {
	int producer_task;			//!< Stores producer task id (task that performs the Send() API )
	int consumer_task;			//!< Stores consumer task id (task that performs the Receive() API )
	int dmni_op;				//!< Stores dmni operation mode (send=0, receive=1) of the ctp
	int subnet;					//!< Stores CS subnet used by this ctp
	char CS_enabled;			//!< Stores 1 when the producer send the ack to the consumer releasing the consumer to make request to the producer using CS
} CTP;


void init_ctp();

CTP * get_ctp_ptr(int, int);

CTP * add_ctp(int, int, int, int);

void remove_ctp(int, int);

void clear_all_ctps_of_task(int);

int get_subnet(int, int, int);

void check_ctp_reconfiguration(TCB *);

void send_NoC_switching_ack(int, int, int, int);

void handle_dynamic_CS_setup(volatile ServiceHeader *);

#endif /* CTP_H_ */
