/*
 * noc_cs_sender.h
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#ifndef NOC_CS_SENDER_H_
#define NOC_CS_SENDER_H_


#include <systemc.h>
#include "../../standards.h"

SC_MODULE(noc_cs_sender){

	//Ports
	sc_in <bool > 		clock;
	sc_in <bool > 		reset;

	sc_in<reg32 > 		data_from_memory;
	sc_in<bool > 		valid;
	sc_out<bool > 		busy;

	// EB and wormhole flow control interface
	sc_out <bool > 		tx;
	sc_out<regCSflit > 	data_out;
	sc_in<bool > 		credit_in;

	//Signals
	sc_signal<reg32 >	data[2];
	sc_signal<bool >	full[2];
	sc_signal<bool > 	head, tail;
	sc_signal<sc_uint<4> > shifter_count;

	void combinational();
	void sequential();

	SC_HAS_PROCESS(noc_cs_sender);
	noc_cs_sender (sc_module_name name_) : sc_module(name_) {

		SC_METHOD(combinational);
		sensitive << full[0] << full[1];
		sensitive << data[0] << data[1];
		sensitive << head;

		SC_METHOD(sequential);
		sensitive << clock.pos();
		sensitive << reset;
	}
};


#endif /* NOC_CS_SENDER_H_ */
