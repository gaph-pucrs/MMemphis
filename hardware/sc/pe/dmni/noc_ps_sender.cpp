/*
 * noc_cs_sender.cpp
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#include "noc_ps_sender.h"

void noc_ps_sender::sequential(){

	if (reset == 1) {
		tail.write(0);
		head.write(0);
		full[0].write(0);
		full[1].write(0);
		data[0].write(0);
		data[1].write(0);
	} else {
		//read from memory
		if (valid.read() == 1 && full[tail.read()].read() == 0 ){
			full[tail.read()].write(1);
			data[tail.read()].write(data_from_memory.read());
			tail.write(!tail.read());
		}

		//write to noc
		if (credit_in.read() == 1 && full[head.read()].read() == 1){
			full[head.read()].write(0);
			head.write(!head.read());
		}
	}
}

void noc_ps_sender::combinational(){
	busy.write( full[0].read() && full[1].read() );
	tx.write( full[0].read() || full[1].read() );
	data_out.write( data[head.read()].read());
}
