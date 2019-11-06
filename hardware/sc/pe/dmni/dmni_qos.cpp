/*
 * dmni_qos.cpp
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#include "dmni_qos.h"


void dmni_qos::comb_update(){

	unsigned int count;
	regSubnet s_active_aux, r_active_aux;
	regSubnet intr_aux = 0;

	//SDN local key configuration
	sdn_local_key_en.write( config_valid.read() && config_code.read() == CODE_LOCAL_KEY );
	sdn_local_key.write(config_data.read());

	s_active_aux = 0;
	r_active_aux = 0;

	for(int i=0; i<SUBNETS_NUMBER; i++){

		s_valid[i].write( s_wheel[i].read() && s_ready[i].read() && !busy[i].read() );
		r_valid[i].write( r_wheel[i].read() && r_ready[i].read() && valid_receive[i].read() );

		s_wheel[i].write( (s_curr == i) && !dmni_mode.read() && !load_mem.read());
		r_wheel[i].write( (r_curr == i) && dmni_mode.read());

		s_active_aux[i] = s_ready[i].read();
		r_active_aux[i] = r_ready[i].read();

		//Nao posso estar recebendo nada e valid receive deve ser 1
		intr_aux[i] = !r_ready[i].read() && valid_receive[i].read();

		s_ready[i].write( !(s_mem_size_reg[i].read() == 0) );
		r_ready[i].write( !(r_mem_size_reg[i].read() == 0) );
	}

	intr_subnet.write(intr_aux);
	send_active.write(s_active_aux);
	receive_active.write(r_active_aux);


	if ( r_valid[r_curr].read() ){
		mem_data_write.write(data_to_write[r_curr].read());
		mem_byte_we.write(0xF);
	} else {
		mem_byte_we.write(0);
	}

	if ( dmni_mode.read() == 0 ){
		mem_address.write( s_mem_address_reg[ s_next ].read() );
	} else {
		mem_address.write( r_mem_address_reg[ r_curr ].read() );
	}

	//Count send
	count = 1;
	for(int i=2; i<SUBNETS_NUMBER; i++){
		if ( s_ready[ ((s_next + i) % SUBNETS_NUMBER) ].read() ){
			count = i;
			break;
		}
	}
	s_next_count = count;

	//Count receive
	count = 0;
	for(int i=1; i<SUBNETS_NUMBER; i++){
		if ( r_ready[ (r_curr + i) % SUBNETS_NUMBER].read() ){
			count = i;
			break;
		}
	}
	r_next_count = count;

}


void dmni_qos::address_size_process(){
	if (reset.read() == 1){
		cs_net_config = 0;
		for(int i=0; i<SUBNETS_NUMBER; i++){
			s_mem_address_reg[i].write(0);
			r_mem_address_reg[i].write(0);
			s_mem_size_reg[i].write(0);
			r_mem_size_reg[i].write(0);
			mem_address2.write(0);
			mem_size2.write(0);
		}
	} else {

		if (config_valid.read() == 1){

			switch (config_code.read()) {

				case CODE_NET:
					cs_net_config = (int) config_data.read();
					break;

				case CODE_OP:
					code_config.write(config_data.read().range(0,0));
					break;

				case CODE_MEM_ADDR2:
					mem_address2.write(config_data.read());
					break;

				case CODE_MEM_SIZE2:
					mem_size2.write(config_data.read());
					break;

				case CODE_MEM_ADDR:

					if (code_config.read() == 1)
						r_mem_address_reg[cs_net_config].write(config_data.read());
					else
						s_mem_address_reg[cs_net_config].write(config_data.read());

					break;

				case CODE_MEM_SIZE:
					if (code_config.read() == 1)
						r_mem_size_reg[cs_net_config].write( config_data.read());
					else
						s_mem_size_reg[cs_net_config].write( config_data.read());

					break;
			}
		}

		//Send address and size update
		if (s_valid[s_curr].read() == 1){

			if (s_mem_size_reg[s_curr].read() == 1){
				if (s_curr == PS_NET_INDEX && mem_size2.read() > 0){

					s_mem_address_reg[s_curr].write(mem_address2.read());
					s_mem_size_reg[s_curr].write(mem_size2.read());
					mem_address2.write(0);
					mem_size2.write(0);

				} else {

					s_mem_address_reg[s_curr].write(0);
					s_mem_size_reg[s_curr].write(0);

				}
			} else {
				s_mem_address_reg[s_curr].write( s_mem_address_reg[s_curr].read() + MEMORY_WORD_SIZE );
				s_mem_size_reg[s_curr].write( s_mem_size_reg[s_curr].read() - 1 );
			}

		}

		if (r_valid[r_curr].read() == 1){

			if (r_mem_size_reg[r_curr].read() == 1){

				r_mem_address_reg[r_curr].write(0);
				r_mem_size_reg[r_curr].write(0);

			} else {
				r_mem_address_reg[r_curr].write( r_mem_address_reg[r_curr].read() + MEMORY_WORD_SIZE );
				r_mem_size_reg[r_curr].write( r_mem_size_reg[r_curr].read() - 1 );
			}
		}
	}
}

void dmni_qos::TDM_wheel_process(){
	if (reset.read() == 1){
		s_curr = 0;
		r_curr = 0;
		s_next = 0;
	} else {
		s_curr = s_next;
		s_next =  (s_next + s_next_count) % SUBNETS_NUMBER;

		r_curr =  (r_curr + r_next_count) % SUBNETS_NUMBER;
	}
}

void dmni_qos::arbiter_process(){
	bool s_ready_all, r_ready_all;

	if (reset.read() == 1){
		dmni_mode.write(0); //init in send mode
		timer.write(0);
	} else {

		timer.write(timer.read() + 1);

		if (dmni_mode.read() == 0)
			load_mem.write(0);
		else
			load_mem.write(1);

		if (timer.read() == 0){

			s_ready_all = 0;
			r_ready_all = 0;

			for(int i=0; i<SUBNETS_NUMBER; i++){
				s_ready_all |= s_ready[i].read();
				r_ready_all |= r_ready[i].read();
			}

			if (s_ready_all != 0 && r_ready_all != 0){
				dmni_mode.write(!dmni_mode.read());
			} else if (r_ready_all != 0){
				dmni_mode.write(1);
			} else {
				dmni_mode.write(0);
			}

		}
	}
}


