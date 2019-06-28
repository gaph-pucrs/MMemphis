#ifndef _CS_CONFIG_H
#define _CS_CONFIG_H

#include <systemc.h>
#include "../../standards.h"

SC_MODULE(CS_config){

	// ports
	sc_in<bool> 	clock;
	sc_in<bool> 	reset;

	sc_in<bool> 	rx;
	sc_in<regflit> 	data_in;
	sc_in<bool> 	credit_o;

	sc_out<bool> 	wait_header;
	sc_out<bool> 	config_en;

	sc_out<sc_uint<3> > config_inport;
	sc_out<sc_uint<3> > config_outport;
	sc_out<regCSnet> 	config_valid;

	//Signals
	sc_signal<regflit> 	payload;
	sc_signal<bool> 	cfg_period;
	sc_signal<bool> 	en;
	sc_signal<regCSnet > subnet;

	enum state {header,p_size,ppayload,config};
	sc_signal<state >		PS;

	void process();
	void comb_update();

	SC_HAS_PROCESS(CS_config);
	CS_config (sc_module_name name_) : sc_module(name_) {

	   SC_METHOD(process);
	   sensitive << reset;
	   sensitive << clock.pos();

	   SC_METHOD(comb_update);
	   sensitive << cfg_period << subnet;
	   sensitive << data_in;
	   sensitive << PS << en;
	}

};

#endif

