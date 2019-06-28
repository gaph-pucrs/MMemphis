#include "CS_config.h"

void CS_config::process(){
   
	if (reset.read()) {
		en.write(0);
		PS.write(header);
	} else {

		if (rx.read() && credit_o.read()) {

			switch (PS.read()) {
				case header:
					if (data_in.read().range(16, 16)) {
						en.write(1);
					}
					PS.write(p_size);
					break;

				case p_size:
					if (en.read()) {
						PS.write(config);
					} else {
						payload.write(data_in.read());
						PS.write(ppayload);
					}
					break;

				case ppayload:
					if (payload.read() == 1) {
						PS.write(header);
					} else
						PS.write(ppayload);
						payload.write(payload.read() - 1);
					break;

				case config:
					en.write(0);
					PS.write(header);
					break;
			}

		}
	}
}

void CS_config::comb_update(){

	if (cfg_period.read() == 1)
		config_valid.write(subnet.read());
	else
		config_valid.write(0);

	wait_header.write( (PS.read() == header) );
	cfg_period.write(  (PS.read() == config) );

	config_en.write(en.read());

	config_inport.write(data_in.read().range(15,13));
	config_outport.write(data_in.read().range(12,10));
	subnet.write(data_in.read().range(CS_SUBNETS_NUMBER,1) );
}
