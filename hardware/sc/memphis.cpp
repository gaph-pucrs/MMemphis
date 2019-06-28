//------------------------------------------------------------------------------------------------
//
//  DISTRIBUTED MEMPHIS -  5.0
//
//  Research group: GAPH-PUCRS    -    contact   fernando.moraes@pucrs.br
//
//  Distribution:  September 2013
//
//  Source name:  memphis.cpp
//
//  Brief description: Control of router position.
//
//------------------------------------------------------------------------------------------------

#include "memphis.h"

int memphis::RouterPosition(int router){
	int pos;
	
	int column = router%N_PE_X;
	
	if(router>=(N_PE-N_PE_X)){ //TOP
		if(column==(N_PE_X-1)){ //RIGHT
			pos = TR;
		}
		else{
			if(column==0){//LEFT
				pos = TL;
			}
			else{//CENTER_X
				pos = TC;
			}
		}
	}
	else{
		if(router<N_PE_X){ //BOTTOM
			if(column==(N_PE_X-1)){ //RIGHT
				pos = BR;
			}
			else{
				if(column==0){//LEFT
					pos = BL;
				}
				else{//CENTER_X
					pos = BC;
				}
			}
		}
		else{//CENTER_Y
			if(column==(N_PE_X-1)){ //RIGHT
				pos = CRX;
			}
			else{
				if(column==0){//LEFT
					pos = CL;
				}
				else{//CENTER_X
					pos = CC;
				}
			}
		}
	}
			
	return pos;
}

regaddress memphis::RouterAddress(int router){
	regaddress r_address;
	
	sc_uint<8> pos_y = (unsigned int) router/N_PE_X;
	sc_uint<8> pos_x = router%N_PE_X;

	r_address[15] = pos_x[7];
	r_address[14] = pos_x[6];
	r_address[13] = pos_x[5];
	r_address[12] = pos_x[4];
	r_address[11] = pos_x[3];
	r_address[10] = pos_x[2];
	r_address[ 9] = pos_x[1];
	r_address[ 8] = pos_x[0];
	r_address[7] = pos_y[7];
	r_address[6] = pos_y[6];
	r_address[5] = pos_y[5];
	r_address[4] = pos_y[4];
	r_address[3] = pos_y[3];
	r_address[2] = pos_y[2];
	r_address[1] = pos_y[1];
	r_address[0] = pos_y[0];
		
	return r_address;	
}


void memphis::pes_interconnection(){
 	int i, p;
 	 	
 	for(i=0;i<N_PE;i++){
		
		//EAST GROUNDING
 		if(RouterPosition(i) == BR || RouterPosition(i) == CRX || RouterPosition(i) == TR){
 			if (io_port[i] != EAST){//If the port in not connected to an IO then:
				credit_i_ps[i][EAST].write(0);
				data_in_ps [i][EAST].write(0);
				rx_ps      [i][EAST].write(0);
 			}

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				credit_i_cs	[i][c][EAST].write(0);
 				data_in_cs	[i][c][EAST].write(0);
 				rx_cs		[i][c][EAST].write(0);
 				req_in		[i][c][EAST].write(0);
 			}

 		//EAST CONNECTION
 		} else{

 			credit_i_ps[i][EAST].write(credit_o_ps[i+1][WEST].read());
 			data_in_ps [i][EAST].write(data_out_ps[i+1][WEST].read());
 			rx_ps      [i][EAST].write(tx_ps      [i+1][WEST].read());

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in		[i][c][EAST].write(req_out	  [i+1][c][WEST].read());
 				credit_i_cs	[i][c][EAST].write(credit_o_cs[i+1][c][WEST].read());
				data_in_cs 	[i][c][EAST].write(data_out_cs[i+1][c][WEST].read());
				rx_cs      	[i][c][EAST].write(tx_cs      [i+1][c][WEST].read());
 			}
 		}
 		
 		//WEST GROUNDING
 		if(RouterPosition(i) == BL || RouterPosition(i) == CL || RouterPosition(i) == TL){
 			if (io_port[i] != WEST){
				credit_i_ps[i][WEST].write(0);
				data_in_ps [i][WEST].write(0);
				rx_ps      [i][WEST].write(0);
 			}

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][WEST].write(0);
 				credit_i_cs[i][c][WEST].write(0);
				data_in_cs [i][c][WEST].write(0);
				rx_cs      [i][c][WEST].write(0);
 			}

 		//WEST CONNECTION
 		} else {

			credit_i_ps[i][WEST].write(credit_o_ps[i-1][EAST].read());
 			data_in_ps [i][WEST].write(data_out_ps[i-1][EAST].read());
 			rx_ps      [i][WEST].write(tx_ps      [i-1][EAST].read());

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][WEST].write(req_out	 [i-1][c][EAST].read());
 				credit_i_cs[i][c][WEST].write(credit_o_cs[i-1][c][EAST].read());
				data_in_cs [i][c][WEST].write(data_out_cs[i-1][c][EAST].read());
				rx_cs      [i][c][WEST].write(tx_cs      [i-1][c][EAST].read());
 			}

 		}
 		
 		//NORTH GROUNDING
 		if(RouterPosition(i) == TL || RouterPosition(i) == TC || RouterPosition(i) == TR){
 			if (io_port[i] != NORTH){
				credit_i_ps[i][NORTH].write(1);
				data_in_ps [i][NORTH].write(0);
				rx_ps      [i][NORTH].write(0);
 			}

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][NORTH].write(1);
 				credit_i_cs[i][c][NORTH].write(1);
 				data_in_cs [i][c][NORTH].write(0);
				rx_cs      [i][c][NORTH].write(0);

 			}

 		//NORTH CONNECTION
 		} else {

			credit_i_ps[i][NORTH].write(credit_o_ps[i+N_PE_X][SOUTH].read());
 			data_in_ps [i][NORTH].write(data_out_ps[i+N_PE_X][SOUTH].read());
 			rx_ps      [i][NORTH].write(tx_ps      [i+N_PE_X][SOUTH].read());

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][NORTH].write(req_out 	  [i+N_PE_X][c][SOUTH].read());
 				credit_i_cs[i][c][NORTH].write(credit_o_cs[i+N_PE_X][c][SOUTH].read());
				data_in_cs [i][c][NORTH].write(data_out_cs[i+N_PE_X][c][SOUTH].read());
				rx_cs      [i][c][NORTH].write(tx_cs      [i+N_PE_X][c][SOUTH].read());
 			}
 		}
 		
 		//SOUTH GROUNDING
 		if(RouterPosition(i) == BL || RouterPosition(i) == BC || RouterPosition(i) == BR){
 			if (io_port[i] != SOUTH){
				credit_i_ps[i][SOUTH].write(0);
				data_in_ps [i][SOUTH].write(0);
				rx_ps      [i][SOUTH].write(0);
 			}

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][SOUTH].write(0);
 				credit_i_cs[i][c][SOUTH].write(0);
				data_in_cs [i][c][SOUTH].write(0);
				rx_cs      [i][c][SOUTH].write(0);
 			}

 		//SOUTH CONNECTION
 		} else {

 			credit_i_ps[i][SOUTH].write(credit_o_ps[i-N_PE_X][NORTH].read());
 			data_in_ps [i][SOUTH].write(data_out_ps[i-N_PE_X][NORTH].read());
 			rx_ps      [i][SOUTH].write(tx_ps      [i-N_PE_X][NORTH].read());

 			for(int c = 0; c < CS_SUBNETS_NUMBER; c++){
 				req_in	   [i][c][SOUTH].write(req_out    [i-N_PE_X][c][NORTH].read());
 				credit_i_cs[i][c][SOUTH].write(credit_o_cs[i-N_PE_X][c][NORTH].read());
				data_in_cs [i][c][SOUTH].write(data_out_cs[i-N_PE_X][c][NORTH].read());
				rx_cs      [i][c][SOUTH].write(tx_cs      [i-N_PE_X][c][NORTH].read());
 			}

 		}

		//--IO Wiring (Memphis <-> IO) ----------------------
 		if (i == APP_INJECTOR && io_port[i] != NPORT) {
 			p = io_port[i];
			memphis_app_injector_tx.write(tx_ps[APP_INJECTOR][p].read());
			memphis_app_injector_data_out.write(data_out_ps[APP_INJECTOR][p].read());
			credit_i_ps[APP_INJECTOR][p].write(memphis_app_injector_credit_i.read());

			rx_ps[APP_INJECTOR][p].write(memphis_app_injector_rx.read());
			memphis_app_injector_credit_o.write(credit_o_ps[APP_INJECTOR][p].read());
			data_in_ps[APP_INJECTOR][p].write(memphis_app_injector_data_in.read());
 		}
 		//Insert the IO wiring for your component here if it connected to a NORTH port:
 	}
}
