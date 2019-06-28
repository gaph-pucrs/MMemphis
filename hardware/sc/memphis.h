//------------------------------------------------------------------------------------------------
//
//  MEMPHIS -  7.0
//
//  Research group: GAPH-PUCRS    -    contact   fernando.moraes@pucrs.br
//
//  Distribution:  2016
//
//  Source name:  memphis.h
//
//  Brief description: to do
//
//------------------------------------------------------------------------------------------------

#include <systemc.h>
#include "standards.h"
#include "pe/pe.h"

#define BL 0
#define BC 1
#define BR 2
#define CL 3
#define CC 4
#define CRX 5
#define TL 6
#define TC 7
#define TR 8



SC_MODULE(memphis) {
	
	sc_in< bool >			clock;
	sc_in< bool >			reset;

	//IO interface - App Injector
	sc_out< bool >			memphis_app_injector_tx;
	sc_in< bool >			memphis_app_injector_credit_i;
	sc_out< regflit >		memphis_app_injector_data_out;

	sc_in< bool >			memphis_app_injector_rx;
	sc_out< bool >			memphis_app_injector_credit_o;
	sc_in< regflit >		memphis_app_injector_data_in;

	//IO interface - Create the IO interface for your component here:



	// Interconnection CS signals
	sc_signal<bool >		tx_cs		[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<regCSflit >	data_out_cs	[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<bool >		credit_i_cs	[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<bool > 		rx_cs		[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<regCSflit >	data_in_cs	[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<bool >		credit_o_cs	[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<bool >		req_in		[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	sc_signal<bool >		req_out		[N_PE][CS_SUBNETS_NUMBER][NPORT-1];
	
	//Interconnection PS signals
	sc_signal<bool >		tx_ps		[N_PE][NPORT-1];
	sc_signal<regflit >		data_out_ps	[N_PE][NPORT-1];
	sc_signal<bool >		credit_i_ps	[N_PE][NPORT-1];
	sc_signal<bool > 		rx_ps		[N_PE][NPORT-1];
	sc_signal<regflit >		data_in_ps	[N_PE][NPORT-1];
	sc_signal<bool >		credit_o_ps	[N_PE][NPORT-1];
		
	pe  *	PE[N_PE];//store slaves PEs
	
	int i,j;
	
	int RouterPosition(int router);
	regaddress RouterAddress(int router);
	regaddress r_addr;
 	void pes_interconnection();
 	
	char pe_name[20];
	int x_addr, y_addr;
	SC_CTOR(memphis){
		
		for (j = 0; j < N_PE; j++) {

			r_addr = RouterAddress(j);
			x_addr = ((int) r_addr) >> 8;
			y_addr = ((int) r_addr) & 0xFF;

			sprintf(pe_name, "PE%dx%d", x_addr, y_addr);
			printf("Creating PE %s\n", pe_name);

			PE[j] = new pe(pe_name, r_addr);
			PE[j]->clock(clock);
			PE[j]->reset(reset);

			//CS signals
			for (int c = 0; c < CS_SUBNETS_NUMBER; c++){

				for (i = 0; i < NPORT - 1; i++) {
					PE[j]->tx_cs		[c][i]	(tx_cs		[j][c][i]);
					PE[j]->data_out_cs	[c][i]	(data_out_cs[j][c][i]);
					PE[j]->credit_i_cs	[c][i]	(credit_i_cs[j][c][i]);
					PE[j]->data_in_cs	[c][i]	(data_in_cs	[j][c][i]);
					PE[j]->rx_cs		[c][i]	(rx_cs		[j][c][i]);
					PE[j]->credit_o_cs	[c][i]	(credit_o_cs[j][c][i]);
					PE[j]->req_in		[c][i]	(req_in		[j][c][i]);
					PE[j]->req_out		[c][i]	(req_out	[j][c][i]);
				}

			}

			//PS signals
			for (i = 0; i < NPORT - 1; i++) {
				PE[j]->tx_ps		[i]	(tx_ps		[j][i]);
				PE[j]->data_out_ps	[i]	(data_out_ps[j][i]);
				PE[j]->credit_i_ps	[i]	(credit_i_ps[j][i]);
				PE[j]->data_in_ps	[i]	(data_in_ps	[j][i]);
				PE[j]->rx_ps		[i]	(rx_ps		[j][i]);
				PE[j]->credit_o_ps	[i]	(credit_o_ps[j][i]);
			}
		}

		SC_METHOD(pes_interconnection);
		sensitive << memphis_app_injector_tx;
		sensitive << memphis_app_injector_credit_i;
		sensitive << memphis_app_injector_data_out;
		sensitive << memphis_app_injector_rx;
		sensitive << memphis_app_injector_credit_o;
		sensitive << memphis_app_injector_data_in;

		for (j = 0; j < N_PE; j++) {

			//CS signals
			for (int c = 0; c < CS_SUBNETS_NUMBER; c++){
				for (i = 0; i < NPORT - 1; i++) {
					sensitive << tx_cs		[j][c][i];
					sensitive << data_out_cs[j][c][i];
					sensitive << credit_i_cs[j][c][i];
					sensitive << data_in_cs	[j][c][i];
					sensitive << rx_cs		[j][c][i];
					sensitive << credit_o_cs[j][c][i];
					sensitive << req_in		[j][c][i];
					sensitive << req_out	[j][c][i];
				}
			}

			//PS signals
			for (i = 0; i < NPORT - 1; i++) {
				sensitive << tx_ps		[j][i];
				sensitive << data_out_ps[j][i];
				sensitive << credit_i_ps[j][i];
				sensitive << data_in_ps	[j][i];
				sensitive << rx_ps		[j][i];
				sensitive << credit_o_ps[j][i];
			}
		}
	}
};

