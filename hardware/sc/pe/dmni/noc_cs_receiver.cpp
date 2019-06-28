/*
 * noc_cs_receiver.cpp
 *
 *  Created on: May 18, 2016
 *      Author: mruaro
 */

#include "noc_cs_receiver.h"

void noc_cs_receiver::sequential(){

	if (reset == 1) {
		tail.write(0);
		head.write(0);
		full[0].write(0);
		full[1].write(0);
		shifter_count.write(MAX_CS_SHIFT);
		data[0].write(0);
		data[1].write(0);
	} else {
		//reads from noc
		if (rx.read() == 1 && full[tail.read()].read() == 0 ){

			data[tail.read()].write( ( data_in, data[tail.read()].read().range(31, TAM_CS_FLIT) ) );

			shifter_count.write(shifter_count.read() - 1);

			if (shifter_count.read() == 1){
				full[tail.read()].write(1);
				shifter_count.write(MAX_CS_SHIFT);
				tail.write(!tail.read());
			}
		}

		//writes to memory
		if (consume.read() == 1 && full[head.read()].read() == 1){
			full[head.read()].write(0);
			head.write(!head.read());
		}
	}
}

void noc_cs_receiver::combinational(){
	credit_out.write( !full[0].read() | !full[1].read() );
	valid.write( full[0].read() | full[1].read() );
	data_to_memory.write( data[head.read()].read() );
}



