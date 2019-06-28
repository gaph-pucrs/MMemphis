#ifndef _CS_ROUTER_h
#define _CS_ROUTER_h

#include <systemc.h>
#include "../../standards.h"

SC_MODULE(CS_router){

	//Ports
	sc_in <bool > 		clock;
	sc_in <bool > 		reset;

	sc_in<bool >		rx[NPORT];
	sc_in<regCSflit >	data_in[NPORT];
	sc_out<bool > 		credit_out[NPORT];
	sc_out<bool > 		req_out[NPORT];

	sc_out<bool >		tx[NPORT];
	sc_out<regCSflit >	data_out[NPORT];
	sc_in<bool > 		credit_in[NPORT];
	sc_in<bool > 		req_in[NPORT];

	sc_in<sc_uint<3> > 	config_inport;
	sc_in<sc_uint<3> > 	config_outport;
	sc_in<bool > 		config_valid;

	//Signals
	sc_signal<sc_uint<3> >	ORT[NPORT];
	sc_signal<sc_uint<3> >	IRT[NPORT];

	sc_signal<regCSflit >	data[NPORT][2];
	sc_signal<bool >		full[NPORT][2];
	sc_signal<bool > 		head[NPORT], tail[NPORT];
	sc_signal<bool > 		req_sig[NPORT];


	void process();
	void combinational();

	SC_HAS_PROCESS(CS_router);
	CS_router (sc_module_name name_) : sc_module(name_) {

		SC_METHOD(process);
		sensitive << reset;
		sensitive << clock.pos();

		SC_METHOD(combinational);
		for ( int i = 0; i < NPORT; i++){
			sensitive << full[i][0] << full[i][1];
			sensitive << data[i][0] << data[i][1];
			sensitive << head[i];
			sensitive << req_sig[i];
		}
	}
};


#endif
