#include "CS_router.h"

void CS_router::process(){
	unsigned int irt;

	if (reset.read()){
		for ( int i = 0; i < NPORT; i++) {
			IRT[i].write(NPORT);
			ORT[i].write(NPORT);
			tail[i].write(0);
			head[i].write(0);
			full[i][0].write(0);
			full[i][1].write(0);
			data[i][0].write(0);
			data[i][1].write(0);
			req_sig[i].write(0);
		}
	} else {

		if (config_valid.read() == 1){

			for ( unsigned int i = 0; i < NPORT; i++) {

				if (i != config_outport.read() && ORT[i].read() == config_inport.read()){
					ORT[i].write(NPORT);
				} else if (i == config_outport.read()){
					ORT[i].write(config_inport.read());
				}

				if (i != config_inport.read() && IRT[i].read() == config_outport.read()){
					IRT[i].write(NPORT);
				} else if (i == config_inport.read()){
					IRT[i].write(config_outport.read());
				}

			}

			//ORT[config_outport.read()].write(config_inport.read());
			//IRT[config_inport.read()].write(config_outport.read());
		}

		for ( int i = 0; i < NPORT; i++) {

			irt = IRT[i].read();

			if (irt < NPORT){
				//receives
				if (rx[i] && !full[i][tail[i]]){
					data[i][tail[i]] = data_in[i];
					full[i][tail[i]] = 1;
					tail[i] = !tail[i];
				}

				//send
				if (credit_in[irt] && full[i][head[i]]){
					full[i][head[i]] = 0;
					head[i] = !head[i];
				}

			} else {
				full[i][0] = 0;
				full[i][1] = 0;
			}

			//Request
			req_sig[i].write(req_in[i].read());

		}

	}
}

void CS_router::combinational(){

	unsigned int ort, irt;

	for ( int i = 0; i < NPORT; i++) {

		ort = ORT[i].read();
		credit_out[i] = (!full[i][0] || !full[i][1]);
		if (ort < NPORT){
			tx[i] = ( full[ort][0] || full[ort][1] );
			data_out[i] = ( data[ort][head[ort]]);
		} else {
			tx[i] = 0;
		}

		irt = IRT[i].read();
		if (irt < NPORT){
			req_out[i].write(req_sig[irt].read());
		} else {
			req_out[i].write(0);
		}


	}
}
