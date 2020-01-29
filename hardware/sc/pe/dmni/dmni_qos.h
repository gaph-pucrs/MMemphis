/*
 * dmni_qos.h
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#ifndef DMNI_QOS_H_
#define DMNI_QOS_H_

#include <systemc.h>
#include "../../standards.h"
#include "noc_ps_sender.h"
#include "noc_ps_receiver.h"
#include "noc_cs_sender.h"
#include "noc_cs_receiver.h"

SC_MODULE(dmni_qos){

	//Ports
	sc_in <bool > 			clock;
	sc_in <bool > 			reset;

	//Configuration interface
	sc_in <bool > 			config_valid;
	sc_in <sc_uint<3> > 	config_code;
	sc_in <reg32>			config_data;

	//Status interface
	sc_out<regSubnet >		intr_subnet;
	sc_out<regSubnet >		send_active;
	sc_out<regSubnet >		receive_active;

	//Memory interface
	sc_out<reg32 >			mem_address;
	sc_out<reg32 >			mem_data_write;
	sc_in<reg32 >			mem_data_read;
	sc_out<reg4 >			mem_byte_we;

	//NoC CS Interface (Local port)
	sc_out<bool > 			tx_cs			[CS_SUBNETS_NUMBER];
	sc_out<regCSflit>		data_out_cs		[CS_SUBNETS_NUMBER];
	sc_in <bool >			credit_in_cs	[CS_SUBNETS_NUMBER];
	sc_in <bool >			rx_cs			[CS_SUBNETS_NUMBER];
	sc_in<regCSflit>		data_in_cs		[CS_SUBNETS_NUMBER];
	sc_out <bool >			credit_out_cs	[CS_SUBNETS_NUMBER];

	//Noc PS Interface (Local port)
	sc_out<bool > 			tx_ps;
	sc_out<regflit>			data_out_ps;
	sc_in <bool>			credit_in_ps;
	sc_in <bool>			rx_ps;
	sc_in<regflit>			data_in_ps;
	sc_out <bool>			credit_out_ps;

	//SDN configuration interface
	sc_out<sc_uint<3> > 	sdn_config_inport;
	sc_out<sc_uint<3> > 	sdn_config_outport;
	sc_out<regCSnet> 		sdn_config_valid;


	//Signals
	//serializers
	sc_signal<bool > 		busy				[SUBNETS_NUMBER];
	sc_signal<bool > 		valid_receive		[SUBNETS_NUMBER];
	sc_signal<reg32> 		data_to_write		[SUBNETS_NUMBER];

	//config registers
	sc_signal<reg32> 		s_mem_address_reg	[SUBNETS_NUMBER];
	sc_signal<reg32> 		s_mem_size_reg		[SUBNETS_NUMBER];
	sc_signal<reg32> 		r_mem_address_reg	[SUBNETS_NUMBER];
	sc_signal<reg32> 		r_mem_size_reg		[SUBNETS_NUMBER];
	sc_signal<bool > 		s_ready				[SUBNETS_NUMBER];
	sc_signal<bool > 		r_ready				[SUBNETS_NUMBER];

	//used only for subnet 0 - Packet Switching
	sc_signal<reg32> 		mem_address2;
	sc_signal<reg32> 		mem_size2;

	//auxiliary
	sc_signal<bool>			code_config;
	sc_signal<bool>			load_mem;
	int 					cs_net_config;

	//combinational TDM wheel control
	sc_signal<int> s_curr, s_next, r_curr, s_next_count, r_next_count;
	sc_signal<bool > 		s_valid[SUBNETS_NUMBER];
	sc_signal<bool >		r_valid[SUBNETS_NUMBER];
	sc_signal<bool >		s_wheel[SUBNETS_NUMBER];
	sc_signal<bool >		r_wheel[SUBNETS_NUMBER];

	//Send and Receive Arbiter
	sc_signal<bool>			dmni_mode;
	sc_signal<reg4>			timer;

	//Instances
	noc_cs_sender	* 	noc_CS_sender	[CS_SUBNETS_NUMBER];
	noc_cs_receiver * 	noc_CS_receiver	[CS_SUBNETS_NUMBER];
	noc_ps_sender	* 	noc_PS_sender;
	noc_ps_receiver * 	noc_PS_receiver;

	//Systemc aux
	char module_name[20];
	//Logical
	//Combinational
	void comb_update();

	//Sequential
	void address_size_process();
	void TDM_wheel_process();
	void arbiter_process();

	SC_HAS_PROCESS(dmni_qos);
	dmni_qos (sc_module_name name_, int pe_addr_) : sc_module(name_), pe_addr(pe_addr_) {

		cs_net_config = 0;

		for(int subnet=0; subnet < SUBNETS_NUMBER; subnet++){

			if (subnet < CS_SUBNETS_NUMBER){

				//Sender CS module
				memset(module_name, 0, sizeof(module_name)); sprintf(module_name,"CS_sender_%d",subnet);
				noc_CS_sender[subnet] = new noc_cs_sender(module_name);
				noc_CS_sender[subnet]->clock(clock);
				noc_CS_sender[subnet]->reset(reset);
				noc_CS_sender[subnet]->data_from_memory(mem_data_read);
				noc_CS_sender[subnet]->valid(s_valid[subnet]);
				noc_CS_sender[subnet]->busy(busy[subnet]);
				noc_CS_sender[subnet]->tx(tx_cs[subnet]);
				noc_CS_sender[subnet]->data_out(data_out_cs[subnet]);
				noc_CS_sender[subnet]->credit_in(credit_in_cs[subnet]);

				//Receiver CS module
				memset(module_name, 0, sizeof(module_name)); sprintf(module_name,"CS_receiver_%d",subnet);
				noc_CS_receiver[subnet] = new noc_cs_receiver(module_name);
				noc_CS_receiver[subnet]->clock(clock);
				noc_CS_receiver[subnet]->reset(reset);
				noc_CS_receiver[subnet]->data_to_memory(data_to_write[subnet]);
				noc_CS_receiver[subnet]->valid(valid_receive[subnet]);
				noc_CS_receiver[subnet]->consume(r_valid[subnet]);
				noc_CS_receiver[subnet]->rx(rx_cs[subnet]);
				noc_CS_receiver[subnet]->data_in(data_in_cs[subnet]);
				noc_CS_receiver[subnet]->credit_out(credit_out_cs[subnet]);
			} else {

				//Sender PS module
				memset(module_name, 0, sizeof(module_name)); sprintf(module_name,"PS_sender_%d",subnet);
				noc_PS_sender = new noc_ps_sender(module_name);
				noc_PS_sender->clock(clock);
				noc_PS_sender->reset(reset);
				noc_PS_sender->data_from_memory(mem_data_read);
				noc_PS_sender->valid(s_valid[subnet]);
				noc_PS_sender->busy(busy[subnet]);
				noc_PS_sender->tx(tx_ps);
				noc_PS_sender->data_out(data_out_ps);
				noc_PS_sender->credit_in(credit_in_ps);

				//Receiver PS module
				memset(module_name, 0, sizeof(module_name)); sprintf(module_name,"PS_receiver_%d",subnet);
				noc_PS_receiver = new noc_ps_receiver(module_name);
				noc_PS_receiver->clock(clock);
				noc_PS_receiver->reset(reset);
				noc_PS_receiver->data_to_memory(data_to_write[subnet]);
				noc_PS_receiver->valid(valid_receive[subnet]);
				noc_PS_receiver->consume(r_valid[subnet]);
				noc_PS_receiver->rx(rx_ps);
				noc_PS_receiver->data_in(data_in_ps);
				noc_PS_receiver->credit_out(credit_out_ps);

				//SDN configuration interface
				noc_PS_receiver->config_inport(sdn_config_inport);
				noc_PS_receiver->config_outport(sdn_config_outport);
				noc_PS_receiver->config_valid(sdn_config_valid);

			}
		}

		//Combinational
		SC_METHOD(comb_update);
		sensitive << dmni_mode;
		for (int i = 0; i < SUBNETS_NUMBER; i++){
			sensitive << s_wheel[i];
			sensitive << r_wheel[i];
			sensitive << s_ready[i];
			sensitive << r_ready[i];
			sensitive << busy[i];
			sensitive << valid_receive[i];
			sensitive << data_to_write[i];
			sensitive << r_valid[i];
			sensitive << s_mem_address_reg[i];
			sensitive << r_mem_address_reg[i];
			sensitive << s_mem_size_reg[i];
			sensitive << r_mem_size_reg[i];
			sensitive << s_curr;
			sensitive << s_next;
			sensitive << r_curr;

			//SDN configuration sensitive
			sensitive << config_valid << config_code;
		}

		//Sequential
		SC_METHOD(address_size_process);
		sensitive << clock.pos();
		sensitive << reset;

		SC_METHOD(TDM_wheel_process);
		sensitive << clock.pos();
		sensitive << reset;

		SC_METHOD(arbiter_process);
		sensitive << clock.pos();
		sensitive << reset;
	}
	public:
		int pe_addr;
};



#endif /* DMNI_QOS_H_ */
