//---------------------------------------------------------------------------------------
//
//  DISTRIBUTED MEMPHIS  - version 5.0
//
//  Research group: GAPH-PUCRS    -    contact   fernando.moraes@pucrs.br
//
//  Distribution:  September 2013
//
//  Source name:  queue.cpp
//
//  Brief description: Control process queue occupancy
//
//---------------------------------------------------------------------------------------

#include "queue.h"


// PROCESSO DE CONTROLE DA OCUPACAO DA FILA
// SE FILA HA ESPACOS LIVRES NA FILA
//   TEM_ESPACO_NA_FILA = TRUE
// DO CONTRARIO
//   TEM_ESPACO_NA_FILA = FALSE
void fila::in_proc_FSM(){
	sc_uint<4> local_first, local_last;
	
	local_first = first.read();
	local_last = last.read();
	
	if(reset_n.read() == false){
		tem_espaco_na_fila.write(true);
			credit_o.write(true);
	}
	else{
		if (((local_first==0) && (local_last==(BUFFER_TAM-2))) || (local_first==(local_last+2)) || (local_first==(local_last+1))){
			tem_espaco_na_fila.write(false);
			credit_o.write(false);
		}
		else if(((local_last - local_first) == 2) || ((local_first - local_last) == BUFFER_TAM-2)){
			tem_espaco_na_fila.write(true);
			credit_o.write(true);
		}
	}
}

// O ponteiro last � inicializado com o valor zero quando o reset � ativado.
// Quando o sinal rx � ativado indicando que existe um flit na porta de entrada �
// verificado se existe espa�o na fila para armazen�-lo. Se existir espa�o na fila o
// flit recebido � armazenado na posi��o apontada pelo ponteiro last e o mesmo �
// incrementado. Quando last atingir o tamanho da fila, ele recebe zero.
void fila::in_proc_updPtr(){
	if(reset_n.read()==false){
		last.write(0);
		sdn_cfg_mode.write(0);
		malicious_received.write(0);
		remaining_cfg_flits.write(0);
		for(int i=0;i<BUFFER_TAM;i++) buffer_in[i]=0;
	}
	else{

		if (malicious_set.read() == 1){
			malicious_received.write(1);
		}


		if((tem_espaco_na_fila.read()==true) && (rx.read()==true)){
//Start of new lines added to support sequential SDN configuration
//Testa se o header possui o endereco do roteador e se o bit 17 ou 16 estao ligados, caso verdade iss
//significa um flit the configuracao.
			if (EA == S_INIT && data_in.read().range(15, 0) == address && data_in.read().range(17, 16) > 0){
				sdn_cfg_mode.write(1);
				cout << "SDN config at router " << hex << address << endl;
			}

			if (sdn_cfg_mode.read() == 1){
				buffer_in[last.read()] = 2; //Payload size of a SDN config packet
				sdn_cfg_mode.write(0);
			} else if (remaining_cfg_flits.read() == 1 && malicious_received.read() == 1){
				buffer_in[last.read()] = 0; //Sets the nack
				//cout << "Malicious SDN cfg" << endl;
				malicious_received.write(0);
			} else {
//End of new lines added to support sequential SDN configuration
				buffer_in[last.read()] = data_in.read(); //This is the only original line
			} //Remove this end if case you remove the new lines added above
			//incrementa o last
			if(last.read()==(BUFFER_TAM - 1))
				last.write(0);
			else
				last.write((last.read() + 1));

			//Sets remaining_cfg_flits
			if (sdn_cfg_mode.read() == 1){
				remaining_cfg_flits.write(data_in.read());
			} else if (remaining_cfg_flits.read() > 0){
				remaining_cfg_flits.write(remaining_cfg_flits.read() - 1);
			}
		}
	}
}

// disponibiliza o dado para transmiss�o.
void fila::out_proc_data()
{

  data.write(buffer_in[first.read()]);

}

// Quando sinal reset � ativado a m�quina de estados avan�a para o estado S_INIT.
// No estado S_INIT os sinais counter_flit (contador de flits do corpo do pacote), h (que
// indica requisi��o de chaveamento) e data_av (que indica a exist�ncia de flit a ser
// transmitido) s�o inicializados com zero. Se existir algum flit na fila, ou seja, os
// ponteiros first e last apontarem para posi��es diferentes, a m�quina de estados avan�a
// para o estado S_HEADER.
// No estado S_HEADER � requisitado o chaveamento (h='1'), porque o flit na posi��o
// apontada pelo ponteiro first, quando a m�quina encontra-se nesse estado, � sempre o
// header do pacote. A m�quina permanece neste estado at� que receba a confirma��o do
// chaveamento (ack_h='1') ent�o o sinal h recebe o valor zero e a m�quina avan�a para
// S_SENDHEADER.
// Em S_SENDHEADER � indicado que existe um flit a ser transmitido (data_av='1'). A m�quina de
// estados permanece em S_SENDHEADER at� receber a confirma��o da transmiss�o (data_ack='1')
// ent�o o ponteiro first aponta para o segundo flit do pacote e avan�a para o estado S_PAYLOAD.
// No estado S_PAYLOAD � indicado que existe um flit a ser transmitido (data_av='1') quando
// � recebida a confirma��o da transmiss�o (data_ack='1') � verificado qual o valor do sinal
// counter_flit. Se counter_flit � igual a um, a m�quina avan�a para o estado S_END. Caso
// counter_flit seja igual a zero, o sinal counter_flit � inicializado com o valor do flit, pois
// este ao n�mero de flits do corpo do pacote. Caso counter_flit seja diferente de um e de zero
// o mesmo � decrementado e a m�quina de estados permanece em S_PAYLOAD enviando o pr�ximo flit
// do pacote.
// Em S_END � indicado que o �ltimo flit deve ser transmitido (data_av='1') quando � recebida a
// confirma��o da transmiss�o (data_ack='1') a m�quina retorna ao estado S_INIT.

void fila::out_proc_FSM(){
	bool local_ack_h;
	bool local_data_ack;
	sc_uint<4> local_first;
	sc_uint<4> local_last;
	regflit	local_counter_flit;
	
	if(reset_n.read()==false){
		data_av.write(false);
		first.write(0);
		EA.write(S_INIT);
		h.write(false);
		counter_flit.write(0);
	}
	else{
		local_ack_h = ack_h.read();
		local_data_ack = data_ack.read();
		local_first = first.read();
		local_last = last.read();
		local_counter_flit = counter_flit.read();
				
		switch(EA.read()){
			case S_INIT:
				counter_flit.write(0);
				h.write(false);
				data_av.write(false);
				if(local_first != local_last){ // detectou dado na fila
					h.write(true);
					EA.write(S_HEADER);
				}
				else{
					EA.write(S_INIT);
				}
			break;
			
			case S_HEADER:
				if(local_ack_h==true){
					EA.write(S_SENDHEADER);      // depois de rotear envia o pacote
					h.write(false);
					data_av.write(true);
					sender.write(true);
				}
				else{
					EA.write(S_HEADER);
				}
			break;

			case S_SENDHEADER:
				if(local_data_ack==true){//confirma��o do envio do header
					//retira o header do buffer e se tem dado no buffer pede envio do mesmo
					if(local_first==(BUFFER_TAM-1)){
						first.write(0);
						if(local_last!=0)
							data_av.write(true);
						else
							data_av.write(false);
					}
					else{
						first.write(local_first + 1);
						if((local_first+1)!=local_last)
							data_av.write(true);
						else
							data_av.write(false);
					}
					EA.write(S_PAYLOAD);
				}
				else{
					EA.write(S_SENDHEADER);
				}
			break;
			  
			case S_PAYLOAD:
				if(local_data_ack==true && local_counter_flit!=1){//confirma��o do envio de um dado que n�o � o tail
					//se counter_flit � zero indica recep��o do size do payload
					if(local_counter_flit == 0)
						counter_flit.write(buffer_in[local_first]);
					else
						counter_flit.write(local_counter_flit - 1);
					//retira um dado do buffer e se tem dado no buffer pede envio do mesmo
					if(local_first == (BUFFER_TAM-1)){
						first.write(0);
						if(local_last!=0)
							data_av.write(true);
						else
							data_av.write(false);
					}
					else{
						first.write(local_first+1);
						if((local_first+1)!=local_last){
							data_av.write(true);
						}
						else{
							data_av.write(false);
						}
					}
					EA.write(S_PAYLOAD);
				}
				else{
					if(local_data_ack==true && local_counter_flit==1){//confirma��o do envio do tail
						//retira um dado do buffer
						if(local_first==(BUFFER_TAM-1)){
							first.write(0);
						}
						else{
							first.write(local_first+1);
						}					
						data_av.write(false);
						sender.write(false);
						EA.write(S_END);
					}
					else{
						if(local_first!=local_last){//se tem dado a ser enviado faz a requisi��o
							data_av.write(true);
							EA.write(S_PAYLOAD);
						}
						else{
							EA.write(S_PAYLOAD);
						}
					}
				}
			break;	

			case S_END:
				data_av.write(false);
				EA.write(S_END2);
			break;
			
			case S_END2://estado necessario para permitir a libera��o da porta antes da solicita��o de novo envio
				data_av.write(false);
				EA.write(S_INIT);
			break;
		}
	}
}
