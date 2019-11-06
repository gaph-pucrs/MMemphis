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
		//if (rx.read() == 1 && full[tail.read()].read() == 0 ){ //Original code
		//Addition of rx_en due the SDN configuration
		if (rx_en.read() == 1 && rx.read() == 1 && full[tail.read()].read() == 0 ){
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
	//Credit is used inside sdn_config_sequential
	credit.write( !full[0].read() || !full[1].read() );

	credit_out.write( !full[0].read() || !full[1].read() );
	valid.write( full[0].read() || full[1].read() );
	data_to_memory.write( data[head.read()].read() );
}


void noc_ps_receiver::sdn_config_combinational(){

	if ( (data_in.read().range(17, 16) > 0 && PS.read() == header) || en_config.read() == 1 || en_set_key == 1){
		rx_en.write(0);
	} else {
		rx_en.write(1);
	}

	if ( (data_in.read().range(31, 16) ^ k1.read()) == k2.read()) {
		key_valid.write(1);
	} else {
		key_valid.write(0);
	}
}

/*
 Configuracao e verificao de autenticidade de configuracao dos roteadores CS
  header -> verifica-se o conteudo do header. Caso este possuir o bit 16 ativo, trata-se
 			  de um pacote de configuracao de CS router. O reg en_config passa para 1 e a SM avanca para p_size.
            Caso o header possuir o flit 17 ativo, trata-se de um pacote de configuracao de key. O reg en_set_key
			  passa para 1 e a SM avanca para p_size.
	p_size -> direciona a SM para o proximo estado apropriado, e tambem configura o reg de payload com o valor de payload do pacote
  ppayload -> apenas consome o pacote ate chegar o ultimo flit, que faz com que a SM retorne para header concluindo o recebimento do pacote
  check_src-> verifica se o 3o flit do pacote (contido em data_in) eh igua a zero. Por padrao so eh possivel configurar a key se o source do pacote
				eh o PE no endereco 0x0. Caso o PE nao for 0x0, gera-se um report the warning e a SM avanca para ppayload para consumir o restante do pacote malicioso
  check_key-> verifica se o key_valid esta com um valor positivo. Caso afirmativo. Caso afirmativo,
              a SM avanca para config. Caso contratio, gera-se um report de warning e a SM avanca para ppayload para consumir o restante do pacote malicioso.
  config ->   configura o CS router. Esta configuracao eh feita pelo reg config_valid que somente eh ativado quando a SM esta em config. Apos a configuracao a SM volta para header.
  set_key ->  configura a key de acordo com o conteudo de data_in. Apos a configuracao a SM volta para header.
*/
void noc_ps_receiver::sdn_config_sequential(){
	if (reset == 1) {
		en_config.write(0);
		en_set_key.write(0);
		PS.write(header);
		payload.write(0);

		//SDN config update
		config_valid.write(0);
		config_inport.write(0);
		config_outport.write(0);
		k1.write(0);
		k2.write(0);
	} else {

		if (rx.read() == 1 && credit.read() == 1){
			switch (PS.read()) {
				case header:
					if ( data_in.read().range(16, 16) ){
						en_config.write(1);
					} else if ( data_in.read().range(17, 17) ){
						en_set_key.write(1);
					}
					PS.write(p_size);
					break;

				case p_size:
					if ( en_config.read() ){
						PS.write(check_key);
					} else if ( en_set_key.read() ){
						PS.write(check_src);
					} else {
						PS.write(ppayload);
					}
					payload.write(data_in.read());
					break;

				case ppayload: //Just consumes the packet
					if ( payload.read() == 1 ){
						PS.write(header);
						en_config.write(0);
						en_set_key.write(0);
					}
					payload.write(payload.read() - 1);
					break;

				case check_src:
					if (data_in.read() == 0){ //Only accepts configuration from PE 0x0
						PS.write(set_key);
					} else {
						cout << "Warning: Malicious config. detected (source different of 0x0)" << endl;
						PS.write(ppayload);
						payload.write(payload.read() - 1);
					}
					break;

				case check_key:
					if ( key_valid.read() ){
						PS.write(config);
					} else {
						cout << "Warning: Malicious config. detected (key does not match)" << endl;
						PS.write(ppayload);
						payload.write(payload.read() - 1);
					}
					break;

				case config:
					en_config.write(0);
					PS.write(header);
					break;

				case set_key:
					en_set_key.write(0);
					PS.write(header);
					break;
			}
		}


		//Update CS routers config
		if ( PS.read() == config ){
			config_valid.write( data_in.read().range(CS_SUBNETS_NUMBER, 1) ); //Gets subnet information from config. packet flit
			config_inport.write(data_in.read().range(15,13)); //Equal to data_in(15 downto 13);
			config_outport.write(data_in.read().range(12,10)); //Equal to data_in(12 downto 10);
		} else {
			config_valid.write(0);
		}

		//Update keys
		if ( PS.read() == set_key ){
			k1.write(data_in.read().range(31, 16));
			k2.write(data_in.read().range(15, 0));
		} else if ( PS.read() == check_key && key_valid.read() ) {
			k1.write(k2.read());
			k2.write(data_in.read().range(15, 0) ^ k2.read());
		} else if ( local_key_en.read() ){
			k1.write(local_key.read().range(31, 16));
			k2.write(local_key.read().range(15, 0));
		}

	}

}



