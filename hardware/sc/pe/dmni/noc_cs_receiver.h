/*
 * noc_cs_receiver.h
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#ifndef NOC_CS_RECEIVER_H_
#define NOC_CS_RECEIVER_H_

#include <systemc.h>
#include "../../standards.h"

SC_MODULE(noc_cs_receiver){

	//Ports
	sc_in <bool > 		clock;
	sc_in <bool > 		reset;

	sc_out<reg32 > 		data_to_memory;
	sc_out<bool > 		valid;
	sc_in <bool > 		consume;

	sc_in <bool > 		rx;
	sc_in<regCSflit > 	data_in;
	sc_out<bool > 		credit_out;

	//Signals
	sc_signal<reg32 >	data[2];
	sc_signal<bool >	full[2];
	sc_signal<bool > 	head, tail;
	sc_signal<sc_uint<4> > shifter_count;

	void combinational();
	void sequential();

	SC_HAS_PROCESS(noc_cs_receiver);
	noc_cs_receiver (sc_module_name name_) : sc_module(name_) {

		SC_METHOD(combinational);
		sensitive << full[0] << full[1];
		sensitive << data[0] << data[1];
		sensitive << head;

		SC_METHOD(sequential);
		sensitive << clock.pos();
		sensitive << reset;
	}
};

#endif /* NOC_CS_RECEIVER_H_ */
