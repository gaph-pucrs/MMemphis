
#ifndef _pe_h
#define _pe_h

#include <systemc.h>
#include "../standards.h"
#include "processor/plasma/mlite_cpu.h"
#include "dmni/dmni_qos.h"
#include "PS_router/router_cc.h"
#include "CS_router/CS_router.h"
#include "CS_config/CS_config.h"
#include "memory/ram.h"

SC_MODULE(pe) {
	
	sc_in< bool >		clock;
	sc_in< bool >		reset;

	// CS NoC Interface
	sc_in<bool > 		rx_cs		[CS_SUBNETS_NUMBER][NPORT-1];
	sc_in<regCSflit >	data_in_cs	[CS_SUBNETS_NUMBER][NPORT-1];
	sc_out<bool >		credit_o_cs	[CS_SUBNETS_NUMBER][NPORT-1];
	sc_out<bool >		req_out		[CS_SUBNETS_NUMBER][NPORT-1];
	sc_out<bool >		tx_cs		[CS_SUBNETS_NUMBER][NPORT-1];
	sc_out<regCSflit >	data_out_cs	[CS_SUBNETS_NUMBER][NPORT-1];
	sc_in<bool >		credit_i_cs	[CS_SUBNETS_NUMBER][NPORT-1];
	sc_in<bool >		req_in		[CS_SUBNETS_NUMBER][NPORT-1];
	
	// PS NoC Interface
	sc_in<bool > 		rx_ps		[NPORT-1];
	sc_in<regflit >		data_in_ps	[NPORT-1];
	sc_out<bool >		credit_o_ps	[NPORT-1];
	sc_out<bool >		tx_ps		[NPORT-1];
	sc_out<regflit >	data_out_ps	[NPORT-1];
	sc_in<bool >		credit_i_ps	[NPORT-1];
	
	//Hold signals
	sc_signal < bool > 			clock_hold;
	bool 						clock_aux;

	//signals
	sc_signal < sc_uint <32 > > cpu_mem_address_reg;
	sc_signal < sc_uint <32 > > cpu_mem_data_write_reg;
	sc_signal < sc_uint <4 > > 	cpu_mem_write_byte_enable_reg;
	sc_signal < sc_uint <32 > > irq_mask_reg;
	sc_signal < sc_uint <32 > > irq_status;
	sc_signal < bool > 			irq;
	sc_signal < sc_uint <32 > > time_slice;
	sc_signal < bool > 			write_enable;
	sc_signal < sc_uint <32 > > tick_counter_local;
	sc_signal < sc_uint <32 > > tick_counter;
	sc_signal < sc_uint <8 > > 	current_page;
	//cpu
	sc_signal < sc_uint <32 > > cpu_mem_address;
	sc_signal < sc_uint <32 > > cpu_mem_data_write;
	sc_signal < sc_uint <32 > > cpu_mem_data_read;
	sc_signal < sc_uint <4 > > 	cpu_mem_write_byte_enable;
	sc_signal < bool > 			cpu_mem_pause;
	sc_signal < bool > 			cpu_enable_ram;
	//Router config
	sc_signal<sc_uint<3> > 		config_r_cpu_inport;
	sc_signal<sc_uint<3> > 		config_r_cpu_outport;
	sc_signal<bool > 			config_r_cpu_valid[CS_SUBNETS_NUMBER];

	//CS config
	sc_signal<sc_uint<3> > 		config_inport_subconfig;
	sc_signal<sc_uint<3> > 		config_outport_subconfig;
	sc_signal<regCSnet > 		config_valid_subconfig;

	//ram
	sc_signal < sc_uint <30 > > addr_a;
	sc_signal < sc_uint <30 > > addr_b;
	sc_signal < sc_uint <32 > > data_read_ram;
	sc_signal < sc_uint <32 > > mem_data_read;


	// DMNI interconnection signals
		//CS
	sc_signal< bool > 			tx_dmni_cs		[CS_SUBNETS_NUMBER];
	sc_signal< regCSflit > 		data_out_dmni_cs[CS_SUBNETS_NUMBER];
	sc_signal< bool > 			credit_i_dmni_cs[CS_SUBNETS_NUMBER];
	sc_signal< bool > 			rx_dmni_cs		[CS_SUBNETS_NUMBER];
	sc_signal< regCSflit > 		data_in_dmni_cs	[CS_SUBNETS_NUMBER];
	sc_signal< bool > 			credit_o_dmni_cs[CS_SUBNETS_NUMBER];
		//CS request wiring
	sc_signal< bool > 			req_out_local	[CS_SUBNETS_NUMBER];
	sc_signal< bool > 			req_in_local	[CS_SUBNETS_NUMBER];
		//CS request control
	sc_signal< regCSnet > 		req_in_reg;
	sc_signal< regCSnet > 		handle_req;
		//PS
	sc_signal< bool > 			tx_dmni_ps;
	sc_signal< regflit > 		data_out_dmni_ps;
	sc_signal< bool > 			credit_i_dmni_ps;
	sc_signal< bool > 			rx_dmni_ps;
	sc_signal< regflit > 		data_in_dmni_ps;
	sc_signal< bool > 			credit_o_dmni_ps;
		//Configuration
	sc_signal <bool > 			cpu_valid_dmni;
	sc_signal <sc_uint<3> > 	cpu_code_dmni;
		//Status
	sc_signal < regSubnet > 	dmni_send_active;
	sc_signal < regSubnet > 	dmni_receive_active;
		//Interruption
	sc_signal < regSubnet > 	dmni_intr;

	//Others DMNI related signals
	sc_signal < sc_uint <32 > > dmni_mem_address;
	sc_signal < sc_uint <32 > > dmni_mem_addr_ddr;
	sc_signal < bool > 			dmni_mem_ddr_read_req;
	sc_signal < sc_uint <4 > > 	dmni_mem_write_byte_enable;
	sc_signal < sc_uint <32 > > dmni_mem_data_write;
	sc_signal < sc_uint <32 > > dmni_mem_data_read;
	sc_signal < bool > 			dmni_enable_internal_ram;

	//Others control signals
	sc_signal < bool > 			reset_n;
	sc_signal < sc_uint <32 > > end_sim_reg;
	sc_signal < sc_uint <32 > > slack_update_timer;
	sc_signal < bool > 			pending_service;

	unsigned char shift_mem_page;

	mlite_cpu	*	cpu;
	ram			* 	mem;
	dmni_qos 	*	dmni;
	router_cc 	*	ps_router;
	CS_router	*	cs_router[CS_SUBNETS_NUMBER];


	/*unsigned long int log_interaction;
	unsigned long int instant_instructions;
	unsigned long int aux_instant_instructions;
	
	unsigned long int logical_instant_instructions;
	unsigned long int jump_instant_instructions;
	unsigned long int branch_instant_instructions;
	unsigned long int move_instant_instructions;
	unsigned long int other_instant_instructions;
	unsigned long int arith_instant_instructions;
	unsigned long int load_instant_instructions;
	unsigned long int shift_instant_instructions;
	unsigned long int nop_instant_instructions;
	unsigned long int mult_div_instant_instructions;*/


	char aux[255];
	FILE *fp;

	//logfilegen *log;
	
	void sequential_attr();
	//void memory_mux();
	//void log_process();
	void comb_assignments();
	void mem_mapped_registers();
	void reset_n_attr();
	void clock_stop();
	void end_of_simulation();
	
	SC_HAS_PROCESS(pe);
	pe(sc_module_name name_, regaddress address_ = 0x00) : sc_module(name_), router_address(address_) {

		end_sim_reg.write(0x00000001);

		shift_mem_page = (unsigned char) (log10(PAGE_SIZE_BYTES)/log10(2));
	
		cpu = new mlite_cpu("cpu", router_address);
		cpu->clk(clock_hold);
		cpu->reset_in(reset);
		cpu->intr_in(irq);
		cpu->mem_address(cpu_mem_address);
		cpu->mem_data_w(cpu_mem_data_write);
		cpu->mem_data_r(cpu_mem_data_read);
		cpu->mem_byte_we(cpu_mem_write_byte_enable);
		cpu->mem_pause(cpu_mem_pause);
		cpu->current_page(current_page);
		
		mem = new ram("ram");
		mem->clk(clock);
		mem->enable_a(cpu_enable_ram);
		mem->wbe_a(cpu_mem_write_byte_enable);
		mem->address_a(addr_a);
		mem->data_write_a(cpu_mem_data_write);
		mem->data_read_a(data_read_ram);
		mem->enable_b(dmni_enable_internal_ram);
		mem->wbe_b(dmni_mem_write_byte_enable);
		mem->address_b(addr_b);
		mem->data_write_b(dmni_mem_data_write);
		mem->data_read_b(mem_data_read);

		dmni = new dmni_qos("dmni_qos", (int) router_address);
		dmni->clock(clock);
		dmni->reset(reset);
		//Configuration interfcace
		dmni->config_valid	(cpu_valid_dmni);
		dmni->config_code	(cpu_code_dmni);
		dmni->config_data	(cpu_mem_data_write_reg);

		//Status outputs
		dmni->intr_subnet	(dmni_intr);
		dmni->send_active	(dmni_send_active);
		dmni->receive_active(dmni_receive_active);

		//Memory interface
		dmni->mem_address	(dmni_mem_address);
		dmni->mem_data_write(dmni_mem_data_write);
		dmni->mem_data_read	(dmni_mem_data_read);
		dmni->mem_byte_we	(dmni_mem_write_byte_enable);

		//NoC CS interface (Local port)
		for ( int c = 0; c < CS_SUBNETS_NUMBER; c++){
			dmni->tx_cs[c]			(tx_dmni_cs[c]);
			dmni->data_out_cs[c]	(data_out_dmni_cs[c]);
			dmni->credit_in_cs[c]	(credit_i_dmni_cs[c]);
			dmni->rx_cs[c]			(rx_dmni_cs[c]);
			dmni->data_in_cs[c]		(data_in_dmni_cs[c]);
			dmni->credit_out_cs[c]	(credit_o_dmni_cs[c]);
		}

		//NoC PS Interface (Local port)
		dmni->tx_ps			(tx_dmni_ps);
		dmni->data_out_ps	(data_out_dmni_ps);
		dmni->credit_in_ps	(credit_i_dmni_ps);
		dmni->rx_ps			(rx_dmni_ps);
		dmni->data_in_ps	(data_in_dmni_ps);
		dmni->credit_out_ps	(credit_o_dmni_ps);

		//SDN configuration interface
		dmni->sdn_config_inport(config_inport_subconfig);
		dmni->sdn_config_outport(config_outport_subconfig);
		dmni->sdn_config_valid(config_valid_subconfig);


		//CS routers wiring
		char module_name[20];
		for ( int c = 0; c < CS_SUBNETS_NUMBER; c++){
			memset(module_name, 0, sizeof(module_name)); sprintf(module_name,"cs_router_%d",c);
			cs_router[c] = new CS_router(module_name);
			cs_router[c]->clock(clock);
			cs_router[c]->reset(reset);

			//CS router <-> NoC assignment
			for(int p = 0; p < NPORT-1; p++){
				cs_router[c]->rx[p]			(rx_cs[c][p]);
				cs_router[c]->data_in[p]	(data_in_cs[c][p]);
				cs_router[c]->credit_out[p]	(credit_o_cs[c][p]);
				cs_router[c]->req_out[p]	(req_out[c][p]);
				cs_router[c]->tx[p]			(tx_cs[c][p]);
				cs_router[c]->data_out[p]	(data_out_cs[c][p]);
				cs_router[c]->credit_in[p]	(credit_i_cs[c][p]);
				cs_router[c]->req_in[p]		(req_in[c][p]);
			}

			//CS Router <-> DMNI assignment
			cs_router[c]->rx[LOCAL]			(tx_dmni_cs[c]);
			cs_router[c]->data_in[LOCAL]	(data_out_dmni_cs[c]);
			cs_router[c]->credit_out[LOCAL]	(credit_i_dmni_cs[c]);
			cs_router[c]->tx[LOCAL]			(rx_dmni_cs[c]);
			cs_router[c]->data_out[LOCAL]	(data_in_dmni_cs[c]);
			cs_router[c]->credit_in[LOCAL]	(credit_o_dmni_cs[c]);

			//CS Router <-> CPU Internal assignment
			cs_router[c]->req_out[LOCAL]	(req_in_local[c]);
			cs_router[c]->req_in[LOCAL]		(req_out_local[c]);
			cs_router[c]->config_inport		(config_r_cpu_inport);
			cs_router[c]->config_outport	(config_r_cpu_outport);
			cs_router[c]->config_valid		(config_r_cpu_valid[c]);
		}

		//PS router assignment
		ps_router = new router_cc("ps_router",router_address);
		ps_router->clock(clock);
		ps_router->reset_n(reset_n);
		ps_router->tick_counter(tick_counter);

		//PS router <-> NoC assignment
		for(int p = 0; p < NPORT-1; p++){
			ps_router->tx[p]		(tx_ps[p]);
			ps_router->credit_o[p]	(credit_o_ps[p]);
			ps_router->data_out[p]	(data_out_ps[p]);
			ps_router->rx[p]		(rx_ps[p]);
			ps_router->credit_i[p]	(credit_i_ps[p]);
			ps_router->data_in[p]	(data_in_ps[p]);
		}
		//PS router <-> DMNI assignment
		ps_router->tx[LOCAL]		(rx_dmni_ps);
		ps_router->credit_o[LOCAL]	(credit_i_dmni_ps);
		ps_router->data_out[LOCAL]	(data_in_dmni_ps);
		ps_router->rx[LOCAL]		(tx_dmni_ps);
		ps_router->credit_i[LOCAL]	(credit_o_dmni_ps);
		ps_router->data_in[LOCAL]	(data_out_dmni_ps);

		/*
		cs_config = new CS_config("cs_config");
		cs_config->clock(clock);
		cs_config->reset(reset);
		cs_config->rx(tx_router_local_ps);
		cs_config->data_in(data_in_dmni_ps);
		cs_config->credit_o(credit_o_dmni_ps);
		cs_config->config_inport(config_inport_subconfig);
		cs_config->config_outport(config_outport_subconfig);
		cs_config->config_valid(config_valid_subconfig);
		cs_config->wait_header(config_wait_header);
		cs_config->config_en(config_en);
		*/

		SC_METHOD(reset_n_attr);
		sensitive << reset;
		
		SC_METHOD(sequential_attr);
		sensitive << clock.pos() << reset.pos();
		
		/*SC_METHOD(log_process);
		sensitive << clock.pos() << reset.pos();*/

		SC_METHOD(comb_assignments);
		sensitive << cpu_mem_address << dmni_mem_address << cpu_mem_address_reg << write_enable;
		sensitive << cpu_mem_data_write_reg << irq_mask_reg << irq_status;
		sensitive << time_slice << tick_counter_local;
		sensitive << dmni_send_active << dmni_receive_active << data_read_ram;
		sensitive << cpu_valid_dmni << cpu_code_dmni << dmni_enable_internal_ram;
		sensitive << mem_data_read << cpu_enable_ram << cpu_mem_write_byte_enable_reg << dmni_mem_write_byte_enable;
		sensitive << dmni_mem_data_write << dmni_intr << slack_update_timer;
		sensitive << cpu_code_dmni << req_in_reg;
		sensitive << config_inport_subconfig << config_outport_subconfig;
		sensitive << data_in_dmni_ps;
		sensitive << config_valid_subconfig;
		
		SC_METHOD(mem_mapped_registers);
		sensitive << cpu_mem_address_reg;
		sensitive << tick_counter_local;
		sensitive << data_read_ram;
		sensitive << time_slice;
		sensitive << irq_status;
		
		SC_METHOD(end_of_simulation);
		sensitive << end_sim_reg;

		SC_METHOD(clock_stop);
		sensitive << clock << reset.pos();	
		
		/*SC_METHOD(memory_mux);
		sensitive << clock.pos();
		sensitive << reset;*/

	}
	
	public:
		regaddress router_address;
};


#endif
