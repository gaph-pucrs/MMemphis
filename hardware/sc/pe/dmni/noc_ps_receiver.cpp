/*
 * noc_cs_receiver.cpp
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#include "noc_ps_receiver.h"

void noc_ps_receiver::sequential(){

	if (reset == 1) {
		tail.write(0);
		head.write(0);
		full[0].write(0);
		full[1].write(0);
		data[0].write(0);
		data[1].write(0);
	} else {
		//reads from noc
		if (rx.read() == 1 && full[tail.read()].read() == 0 ){
			data[tail.read()].write( data_in.read() );
			full[tail.read()].write(1);
			tail.write(!tail.read());
		}

		//writes to memory
		if (consume.read() == 1 && full[head.read()].read() == 1){
			full[head.read()].write(0);
			head.write(!head.read());
		}
	}
}

void noc_ps_receiver::combinational(){
	credit_out.write( !full[0].read() || !full[1].read() );
	valid.write( full[0].read() || full[1].read() );
	data_to_memory.write( data[head.read()].read() );
}



