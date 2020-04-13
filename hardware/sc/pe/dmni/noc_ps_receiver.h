/*
 * noc_cs_receiver.h
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#ifndef NOC_PS_RECEIVER_H_
#define NOC_PS_RECEIVER_H_

#include <systemc.h>
#include "../../standards.h"

SC_MODULE(noc_ps_receiver){

	//Ports
	sc_in <bool > 		clock;
	sc_in <bool > 		reset;

	sc_out<reg32 > 		data_to_memory;
	sc_out<bool > 		valid;
	sc_in <bool > 		consume;

	sc_in <bool > 		rx;
	sc_in<regflit > 	data_in;
	sc_out<bool > 		credit_out;

	//SDN configuration interface
	sc_out<sc_uint<3> > 	config_inport;
	sc_out<sc_uint<3> > 	config_outport;
	sc_out<regCSnet> 		config_valid;

	sc_out<bool>		malicious_cfg;


	//Signals
	sc_signal<reg32 >	data[2];
	sc_signal<bool >	full[2];
	sc_signal<bool > 	head, tail;

	//SDN configuration signals
	sc_signal<regflit> 	payload;
	sc_signal<reg16> 	k1, k2;
	sc_signal<bool> 	en_config, en_set_key, key_valid;
	sc_signal<bool> 	credit, rx_en;

	enum state {header, p_size, ppayload, check_src, check_key, config, set_key};
	sc_signal<state >	PS;

	void combinational();
	void sequential();

	void sdn_config_combinational();
	void sdn_config_sequential();

	SC_HAS_PROCESS(noc_ps_receiver);
	noc_ps_receiver (sc_module_name name_) : sc_module(name_) {

		SC_METHOD(combinational);
		sensitive << full[0] << full[1];
		sensitive << data[0] << data[1];
		sensitive << head;

		SC_METHOD(sequential);
		sensitive << clock.pos();
		sensitive << reset;

		//New SDN configuration methods
		SC_METHOD(sdn_config_combinational);
		sensitive << data_in;
		sensitive << PS;
		sensitive << en_config;
		sensitive << en_set_key;
		sensitive << k1 << k2;

		SC_METHOD(sdn_config_sequential);
		sensitive << clock.pos();
		sensitive << reset;
	}
};

#endif /* NOC_CS_RECEIVER_H_ */
