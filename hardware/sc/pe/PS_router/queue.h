//---------------------------------------------------------------------------------------
//
//  DISTRIBUTED MEMPHIS  - version 5.0
//
//  Research group: GAPH-PUCRS    -    contact   fernando.moraes@pucrs.br
//
//  Distribution:  September 2013
//
//  Source name:  queue.h
//
//  Brief description: Methods of process control queue occupancy
//
//---------------------------------------------------------------------------------------

#ifndef _fila_h
#define _fila_h

#include <systemc.h>
#include "../../standards.h"

SC_MODULE(fila){

  sc_in<bool > clock;
  sc_in<bool > reset_n;
  sc_in<regflit > data_in;
  sc_in<bool > rx;
  sc_out<bool > credit_o;
  //sc_out<bool > ack_rx;
  sc_out<bool > h;
  sc_in<bool > ack_h;
  sc_out<bool > data_av;
  sc_out<regflit > data;
  sc_in<bool > data_ack;
  sc_out<bool > sender;

  sc_in<bool > malicious_set;

  enum fila_out{S_INIT, S_PAYLOAD, S_SENDHEADER, S_HEADER, S_END, S_END2};
  sc_signal<fila_out > EA, PE;

  regflit buffer_in[BUFFER_TAM];

  sc_signal<sc_uint<4> >  first,last;
  sc_signal<bool > tem_espaco_na_fila, auxack_rx;
  sc_signal<regflit > counter_flit;
  sc_signal<bool > sdn_cfg_mode;

  sc_signal<reg32 > remaining_cfg_flits;
  sc_signal<bool > malicious_received;

  void in_proc_FSM();
  void in_proc_updPtr();

  void out_proc_data();
  void out_proc_FSM();
  void change_state_sequ();
  void change_state_comb();

  SC_HAS_PROCESS(fila);
  	fila(sc_module_name name_, regaddress my_address_ = 0x0000) :
  	sc_module(name_), address(my_address_) {

		SC_METHOD(in_proc_FSM);
		sensitive << reset_n.neg();
		sensitive << clock.pos();

		SC_METHOD(out_proc_data);
		sensitive << first;
		sensitive << last;

		SC_METHOD(out_proc_FSM);
		sensitive << reset_n.neg();
		sensitive << clock.pos();

		SC_METHOD(in_proc_updPtr);
		sensitive << reset_n.neg();
		sensitive << clock.neg();
  }
  private:
	regaddress address;

};

#endif
