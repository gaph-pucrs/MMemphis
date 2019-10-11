/*
 * noc_manager.c
 *
 *  Created on: 26 de set de 2018
 *      Author: mruaro
 */
#include <api.h>
#include "SDN_includes/noc_manager.h"
#include "SDN_includes/connection_request.h"
#include "SDN_includes/global_connection_request_queue.h"
#include "SDN_includes/local_connection_request_queue.h"
#include "SDN_includes/token_queue.h"
#include "SDN_includes/print_path.h"

#define CLUSTER_ID 0

enum ControllerStates {IDLE, GLOBAL_SLAVE, GLOBAL_MASTER};
enum ControllerStates controller_status;

ConnectionRequest 	global_path_request;
unsigned int 		controllers_response_counter;
unsigned int 		token_coordinator_address = 0x000;
unsigned char 		token_requested = 0;
unsigned char		global_routing_attempts;
unsigned int 		path_overhead;

/**** Implicit declaration warnings remotion *****/
void build_border_status();
void handle_token_request(unsigned int);
void handle_token_grant();
void handle_token_release();
void send_update_border_request();
void global_path_release(int, int, int, int, int, int);
void global_routing();
void update_cluster_border(int, int, unsigned int *);
void handle_component_request(int, int, int, int);
void handle_local_release_ack(unsigned int, int, int);
void init_cluster_address_offset(unsigned int);

/*################################## TOKEN FUNCTIONS #####################################*/
void send_token_request(){
	unsigned int * message = get_message_slot();

#if SDN_DEBUG
		Puts("Cluster sent token request to cluster:"); Puts(itoa(token_coordinator_address)); Puts("\n");
#endif
	
	token_requested = 1;
		
	if (token_coordinator_address != cluster_addr){
		message[0] = TOKEN_REQUEST;
		message[1] = cluster_addr;
		send(token_coordinator_address, message, 2);//message size == 2
	} else {
		handle_token_request(cluster_addr);
	}

}

void send_token(unsigned int requester){
	unsigned int * message = get_message_slot();

	if (requester != cluster_addr){
		message[0] = TOKEN_GRANT;
		message[1] = cluster_addr;
#if SDN_DEBUG
		Puts("Token message sent to"); Puts(itoa(requester)); Puts("\n");
#endif
		send(requester, message, 2);
	} else {
#if SDN_DEBUG
		Puts("Send token to myself\n");
#endif
		handle_token_grant();
	}
}

/**This function in on the controller side
 *
 */
void send_token_release(){
	unsigned int * message = get_message_slot();

	if (controller_status == GLOBAL_MASTER){
		controller_status = IDLE;
#if SDN_DEBUG
		Puts("Controller IDLE 1d\n");
#endif
	} else {
		Puts("ERROR: controller not in GLOBAL_MASTER\n");
		while(1);
	}
	
	if (cluster_addr != token_coordinator_address){
		message[0] = TOKEN_RELEASE;
		message[1] = cluster_addr;
		send(token_coordinator_address, message, 2);
	} else {
		handle_token_release();
	}

}

/**This function in on the coordinator side
 *
 */
void handle_token_release(){

#if SDN_DEBUG
	Puts("Handle token release\n");
#endif
	
	token = get_next_token_request();

	if (token != -1){ //This means that there is a requester waiting
		send_token(token);
	}
}

/**This function in on the coordinator side
 *
 */
void handle_token_request(unsigned int requester){
	
#if SDN_DEBUG
	Puts("Cluster "); Puts(itoa(cluster_id)); Puts("handle token request from cluster id "); Puts(itoa(requester)); Puts("\n");
#endif
	
	if (token == -1){//This means that token is available
		send_token(requester);
		token = requester;
	} else {
#if SDN_DEBUG
		Puts("Token request added in FIFO"); Puts(itoa(requester)); Puts("\n");
#endif
		add_token_request(requester);
	}
}
/*################################## END TOKEN FUNCTIONS #####################################*/


void handle_token_grant(){

#if SDN_DEBUG
	Puts("Token granted\n");
#endif
	
	//Overhead for global path
	path_overhead = GetTick();

	if(controller_status == IDLE && token_requested){
		controller_status = GLOBAL_MASTER;
#if SDN_DEBUG
		Puts("Controller GLOBAL_MASTER - master_begin\n");
#endif
	} else {
		Puts("ERROR - controller was not IDLE\n");
		while(1);
	}

	token_requested = 0;

	if (global_path_request.subnet == -1){//-1 means path request, otherwise means path release

		send_update_border_request();

	} else {
		controllers_response_counter = 1; //Response is set to 1 because the coordinator don't calls the response function
		global_path_release(LOCAL_IN, global_path_request.source, global_path_request.target, 1, cluster_addr, global_path_request.subnet);
	}
}


#if SDN_DEBUG
void print_input_borders(int subnet){
	Puts("\nSUBNET: "); Puts(itoa(subnet)); Puts("\nInput border:\n\t\tE\tW\tN\tS\n");
	for(int x=0; x<SDN_X_CLUSTER_NUM; x++){

		for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){

			Puts("Cluster "); Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(" : ");

			for(int k=0; k<4; k++){ //4 is the number of border in each cluster

				Puts("\t"); Puts(itoa(cluster_border_input[x][y][subnet][k]));
			}
			Puts("\n");
		}
	}
}

void print_output_borders(int subnet){
	Puts("\nSUBNET: "); Puts(itoa(subnet)); Puts("\nOutput border:\n\t\tE\tW\tN\tS\n");
	for(int x=0; x<SDN_X_CLUSTER_NUM; x++){

		for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){

			Puts("Cluster "); Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(" : ");

			for(int k=0; k<4; k++){ //4 is the number of border in each cluster

				Puts("\t"); Puts(itoa(cluster_border_output[x][y][subnet][k]));
			}
			Puts("\n");
		}
	}
}

void print_subnet_utilization(){
	Puts("\nSubnet utilization\n\t");
	for(int s=0; s<CS_NETS; s++){ //4 is the number of border in each cluster
		Puts("\t"); Puts(itoa(s));
	}
	for(int x=0; x<SDN_X_CLUSTER_NUM; x++){
		for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){
			Puts("\nCluster "); Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(" : ");
			for(int s=0; s<CS_NETS; s++){ //4 is the number of border in each cluster
				Puts("\t"); Puts(itoa(global_subnet_utilization[x][y][s]));
			}
		}
	}
	Puts("\n");
}
#endif

void handle_update_border_request(unsigned int coordinator_addr){

	unsigned int * message = get_message_slot();
	int msg_index = 0;

	build_border_status();

	message[msg_index++] = UPDATE_BORDER_ACK;
	message[msg_index++] = cluster_addr;
	message[msg_index++] = cluster_id;

	//Sends subnet
#if SDN_DEBUG
	Puts("Subnet utilization sent:\n");
#endif
	for(int s=0; s<CS_NETS; s++){
#if SDN_DEBUG
		Puts(itoa(s)); Puts(": "); Puts(itoa(subnet_utilization[s])); Puts("\n");
#endif
		message[msg_index++] = subnet_utilization[s];
	}

	//Sends border status
	for(int s=0; s<CS_NETS; s++){
		for(int i=0; i<4; i++){
			message[msg_index++] = cluster_border_input[x_cluster_addr][y_cluster_addr][s][i];
			message[msg_index++] = cluster_border_output[x_cluster_addr][y_cluster_addr][s][i];
		}
	}
	send(coordinator_addr, message, msg_index);
	
#if SDN_DEBUG
	Puts("\nMessage UPDATE_BORDER_STATUS_ACK sent to coordinator "); Puts(itoa(coordinator_addr)); Puts("\n");
#endif

}

//Given a PE address, tells if some path can made from that PE
unsigned short int is_PE_propagable(int x, int y, int border, int s){

	//printf("\nTesting propagation of PE: %dx%d at border %d", x, y, border);
	//Test if the input port is used
	if (cs_inport[x][y][s][border] != -1)
		return 0;

	//Test if router can send to WEST
	if (x > 0 && cs_inport[x-1][y][s][EAST] == -1)
		return 1;

	//Test if router can send to EAST
	if (x < (SDN_XCLUSTER-1) && cs_inport[x+1][y][s][WEST] == -1)
		return 1;

	//Test if router can send to SOUTH
	if (y > 0 && cs_inport[x][y-1][s][NORTH] == -1)
		return 1;

	//Test if router can send to NORTH
	if (y < (SDN_YCLUSTER-1) && cs_inport[x][y+1][s][SOUTH] == -1)
		return 1;

	return 0;
}

//Given a PE address, tells if some path reach to that PE
unsigned short int is_PE_reachable(int x, int y, int s){
	//Test if PE is at EAST corner
	if (x > 0 && cs_inport[x][y][s][WEST] == -1)
		return 1;

	if (x < (SDN_XCLUSTER-1) && cs_inport[x][y][s][EAST] == -1)
		return 1;

	//Test if PE is at NORTH corner
	if (y > 0 && cs_inport[x][y][s][SOUTH] == -1)
		return 1;

	if (y < (SDN_YCLUSTER-1) && cs_inport[x][y][s][NORTH] == -1)
		return 1;

	return 0;
}

void send_ack_requester(int path_success, int source, int target, int requester_addr, int is_global, enum ReqMode mode){
	
#if SDN_DEBUG
	Puts("\nMessage ACK sent to requester\n");
#endif
	
	unsigned int * message = get_message_slot();
	int path_uint_size = 0;
	
	path_overhead = GetTick() - path_overhead;

	message[0] = PATH_CONNECTION_ACK;
	message[1] = cluster_addr;
	//    16 bits                    16 bits
	// source_controller_addr  |   path id
	if (path_success != -1){
		message[2] = 1;

		//Apagar essa linha de baixo na hemps
		if (mode == ESTABLISH){
#if SDN_DEBUG
			Puts("Begin write path\n");
#endif
			
#if PATH_DEBUG
			path_uint_size = write_path(global_path_size, &message[8]);
#endif
			
#if SDN_DEBUG
			Puts("End write path\n");
#endif
		}
		//Apagar ate aqui
	} else {
		message[2] = 0;
	}

#if PATH_DEBUG
	//Apagar aqui tambem, o clear_print() zera estrutura do print patj
	clear_print();
#endif

	message[3] = source;
	message[4] = target;
	message[5] = path_success; //subnet
	message[6] = path_overhead;
	message[7] = is_global;
#if PATH_DEBUG == 0
	message[8] = printPathSize;
	path_uint_size += 9;
	printPathSize = 0;
#else
	path_uint_size += 8;
#endif


	SendService(requester_addr, message, path_uint_size);
	//A flag TO_KERNEL faz com que o endereço seja enviado diretamente pro kernel
	//send((TO_KERNEL | requester_addr), message, path_uint_size);
	
}


void send_update_border_request(){

	unsigned int clust_addr_aux;
	unsigned int * message;

	//Update its own border
	build_border_status();

	controllers_response_counter = 1;

	for(int x=0; x<SDN_X_CLUSTER_NUM; x++){
		for (int y = 0; y<SDN_Y_CLUSTER_NUM; y++){


			clust_addr_aux = x << 8 | y; //Composes the cluster address

			if (clust_addr_aux != cluster_addr){
				
				message = get_message_slot();
				message[0] = UPDATE_BORDER_REQUEST;
				message[1] = cluster_addr;
#if SDN_DEBUG
				Puts("Send UPDATE_BORDER_REQUEST to cluster "); Puts(itoa(clust_addr_aux)); Puts("\n");
#endif
				send(clust_addr_aux, message, 2);
				
#if SDN_DEBUG
				Puts("sent!\n");
#endif
			}
		}
	}

}


/**Coordinator controller handle the CONFIGURE_PATH_RESPONSE sent from the controllers
 *
 */
void handle_global_mode_release_ack(unsigned int * msg){

	int coordinator_addr;

	controllers_response_counter++;

	coordinator_addr = msg[1];

#if SDN_DEBUG
	Puts("\nMessage GLOBAL_MODE_RELEASE_ACK received from cluster "); Puts(itoa(coordinator_addr)); Puts("\n");
#endif
	
#if PATH_DEBUG
	addPath(msg[1], &msg[3], msg[2]);
#else
	printPathSize += msg[2];
#endif

	if (controllers_response_counter == NC_NUMBER){

#if SDN_DEBUG
		if (global_path_request.subnet != -1){
			Puts("\n\n\tPATH CONFIGURATION PROCESS FINISHEED\n\n");
		}
#endif
		
		send_ack_requester(global_path_request.subnet, global_path_request.source, global_path_request.target, global_path_request.requester_address, 1, ESTABLISH);

		global_path_size = 0;
		
		send_token_release();

#if SDN_DEBUG
		Puts("Controller IDLE\n");
#endif
	}
}

/*
 * Deve ser eviado no Total Path failure e no lugar do configure path
 *
 * - For each controller in the system
 *    - Test if the path is sucess
 *       - if YES, test if the controller belongs to the path
 *          	- If yes Get the inport and outport
 *       	- If the controller not belongs to the path
 *          	- Sets the message[1] = NOT_CONFIGURE
 *       	- ElseIf if the controller is not the current one
 *          	- Set the message parameters to configure the path
 *       	- Else (If the controller is the current)
 *          	- Set the inport and outport to the my ports
 *   	 - if NOT set the message[1] = NOT_CONFIGURE
 *
 */
void send_global_mode_release(int path_success){

	unsigned int curr_x, curr_y, clust_addr_aux;
	unsigned int * message;
	int inport, outport;
	int my_inport, my_outport;

	//Resets the controller response counter
	controllers_response_counter = 1;

	my_inport = -1;
	my_outport = -1;

	for(int x=0; x<SDN_X_CLUSTER_NUM; x++){
		for (int y = 0; y<SDN_Y_CLUSTER_NUM; y++){

			inport = -1;
			//Searches if the cluster belongs to the path
			if (path_success){
				//Test if the current cluster belongs to the path
				for(int i=global_path_size-1; i>=0; i--){
					curr_x = (cluster_path[i] & 0xFF000000) >> 24;//Extracts the cluster X address
					curr_y = (cluster_path[i] & 0x00FF0000) >> 16;//Extracts the cluster Y address

					//Else the cluster was found
					if (curr_x == x && curr_y == y){
						inport = (cluster_path[i] & 0x0000FF00) >> 8;//Extracts the inport
						outport = cluster_path[i] & 0x000000FF;//Extracts the outport
						break;
					}
				}
			}

			clust_addr_aux = x << 8 | y;

			//Cluster belongs
			if (inport != -1){

				if (clust_addr_aux != cluster_addr){

					message = get_message_slot();
					message[0] = GLOBAL_MODE_RELEASE;
					message[1] = 1; //Configure
					message[2] = cluster_addr;
					message[3] = inport;
					message[4] = outport;
					message[5] = global_path_request.subnet;
					send(clust_addr_aux, message, 6);
#if SDN_DEBUG
					Puts("Send GLOBAL_MODE_RELEASE to cluster "); Puts(itoa(clust_addr_aux)); Puts(" - YES CONFIGURE\n");
#endif

				} else {
					my_inport = inport;
					my_outport = outport;
#if SDN_DEBUG
					Puts("My port defined!\n");
#endif
				}

			} else if (clust_addr_aux != cluster_addr){

				message = get_message_slot();
				message[0] = GLOBAL_MODE_RELEASE;
				message[1] = 0; //not configure anything
				message[2] = cluster_addr;
				send(clust_addr_aux, message, 3);
#if SDN_DEBUG
				Puts("Send GLOBAL_MODE_RELEASE to cluster "); Puts(itoa(clust_addr_aux)); Puts(" - NOT CONFIGURE\n");
#endif

			}

		}
	}

	//Retrace
	if (path_success){

		while(my_inport == -1) Puts("ERROR: my port not defined\n");
		//The retrace phase will defitivelly configure the paths by retrancing the path previosly configured by hadlock
#if SDN_DEBUG
		Puts("\n********** retrace coordinator *********");
#endif
		retrace(global_path_request.subnet, my_inport, my_outport);

#if SDN_DEBUG
		//print_router_status(global_path_request.subnet);
		Puts("*****************************************\n");
		Puts("\n");
#endif

#if PATH_DEBUG
		addPath(cluster_addr, print_paths, print_path_size);
#else
		printPathSize += print_path_size;
#endif
	}

}

void send_detailed_routing_response(int path_found, unsigned int coordinator_address, unsigned int entry_border, unsigned int entry_router){
	unsigned int * message = get_message_slot();

	message[0] = DETAILED_ROUTING_RESPONSE;
	message[1] = path_found;
	message[2] = cluster_addr;
	message[3] = entry_border;
	message[4] = entry_router;

	send(coordinator_address, message, 5);

#if SDN_DEBUG
	Puts("Send DETAILED_ROUTING_RESPONSE message to "); Puts(itoa(coordinator_address)); Puts("\n");
#endif

}

/**Called even when the coordinator controller receives a response of anothe controller about the sucess or failure in detailed routing
 *
 */
void handle_detailed_routing_response(int response, int source_controller, unsigned int entry_border, unsigned int entry_router){

	int x, y;
	static int all_detailed_routing_ok = 1; //0 - Some path cannot be done, 1 - All paths sucessifuly established

#if SDN_DEBUG
	Puts("Received WAITING_DETAILED_ROUTING_RESPONSE from "); Puts(itoa(source_controller)); Puts("\n");
#endif

	//Tests if the detailed routing status if positive so far, and if the controller received a false response
	if (!response){
		//At this point is possible to conclude that the path cannot be defined because one of the controllers returned a false answer
		//One of the options can be to change the SUBNET
#if SDN_DEBUG
		Puts("PATH NOT FOUND by coordinator "); Puts(itoa(source_controller)); Puts("!\n\n");

		
		if (available_controllers[source_controller >> 8][source_controller & 0xFF][global_path_request.subnet])
			Puts("Controller "); Puts(itoa(source_controller >> 8)); Puts("x"); Puts(itoa(source_controller & 0xFF)); Puts("were enable but now was disabled\n");
#endif
		
		available_controllers[source_controller >> 8][source_controller & 0xFF][global_path_request.subnet] = 0;


		/*Aqui tem outro porem, eu nao posso suspender totalmente a busca se a condição abaixo for verdade, isso pq
		 * ainda podem haver caminhos em outros planos. Devo que somente suspender as buscas se todos os planos foram
		 * esgotados já.
		 */
		x = (cluster_path[0] & 0xFF000000) >> 24;//Extracts the cluster X address
		y = (cluster_path[0] & 0x00FF0000) >> 16;//Extracts the cluster Y address
		//Disable controller if it is diferent form source or target controller
		//Disable target controller as a techinique to to avoid the use of this subnet in future global routing for the same source->target
#if SDN_DEBUG
		Puts("Source controller: "); Puts(itoa(cluster_addr)); Puts(", target controller: "); Puts(itoa((x << 8 | y))); Puts("\n");
#endif
		if (source_controller == cluster_addr || source_controller == (x << 8 | y)){
#if SDN_DEBUG
			Puts("Controller is equal to source or target controller, disabling contrller target controller\n");
#endif
			available_controllers[x][y][global_path_request.subnet] = 0;
		}

		all_detailed_routing_ok = 0;
	}


	controllers_response_counter++;

	//printf("Glbal paht size: %d\n", controllers_response_counter);

	if (controllers_response_counter == global_path_size){//This means that all controllers returned a positive answer
		//When all cluster serached the path, them it is time to configure the cluster routers physically

#if SDN_DEBUG
		Puts("All controller sent the response!!!\n");
#endif

		if (all_detailed_routing_ok){ //If the status of all path was OK then proceceed to configure phisically each path

#if SDN_DEBUG
			Puts("Positive response!!!\nStarting the path configuration...\n");
#endif
			
			//Start the process to controll the cluster configuration OK
			controllers_response_counter = 1; //Reset the controller to now count the number of controller that reponse OK for path configuration
			//controllers_response_counter is reseted with 1 to take in account the own coordinator controller

			//configure_path_to_all_controllers();
			//Release and configure controllers
			send_global_mode_release(1);

		} else {

			all_detailed_routing_ok = 1;

#if SDN_DEBUG
			Puts("NEGATIVE response for the path, trying again\n");
#endif
			//Tryies agains the global routing with the failed cluster removed from path search
			//send_global_mode_release(0);
			
			//This variable will be used inside global routing to not active the minimal path mode when attempts are higher them 1
			global_routing_attempts++;

			global_routing();

		}
	}
}

//					SDN_XCLUSTER		x_offset			input_port		//target_x
void heuristic(int border_lenght, int addr_offset, int port_status, int * addr, int axis_ref){
	unsigned int bit_mask, curr_addr;
	int min_axis_ref, curr_axis_ref;

	min_axis_ref = XDIMENSION*YDIMENSION;//Initializes with a impossible size

	for(int i=0; i<border_lenght; i++){
		bit_mask = 1 << i;
		//Testa se a porta esta livre, e atribui a porta candidata se o bit atual eh igual a -1 ou se o endereco
		//encontra-se dentro do quadrado envolvente

		//|---------|  0
		//|---------|  |
		//|---------| \|/
		//|---------| 32
		//32 <----- 0

		//Uma vez que a busca ocorre da direita para esquerda ou de cima pra baixo, o curr_addr comeca valendo o valor maximo
		//do roteador da esquerda, sendo decrementado ate chegar ao seu proprio valor
		curr_addr = addr_offset + (border_lenght-1)-i;
		curr_axis_ref = abs(curr_addr - axis_ref);
		//   porta esta livre         E  (nenhuma porta escolhida   OU   O valor do eixo esta o mais proximo do respecitvo eixo do target)
		if ((!(port_status & bit_mask)) && (*addr == -1           || (curr_axis_ref < min_axis_ref) )){
			*addr = curr_addr;
			min_axis_ref = curr_axis_ref;
		}
	}
}

/**Essa funcao recebe um pedido do Controller coordenador e faz as seguintes coisas:
 * 1. Baseado no quadrado envolvente do X e Y, define o entry e exit point dentro desses quadrados
 * 2. Aplica o Hadlock com o entry e exit point
 * 3. Devolve o resultado do Hadlock pro coordenador.
 */
void detailed_routing(unsigned int * msg){

	int coordinator_address, source, target, entry_border, exit_boder, selected_subnet;
	int source_x, source_y, target_x, target_y, detailed_routing_ret;
	int ref_x, ref_y;
	unsigned short int input_status = 0, output_status = 0;

#if SDN_DEBUG
	Puts("\n************ handle detailed routing **************\n");
#endif

	coordinator_address = msg[1];
	source = msg[2];
	target = msg[3];
	selected_subnet = msg[4];
	entry_border = msg[5];
	exit_boder = msg[6];

	detailed_routing_ret = 1;//Early bloked is a flag used to test if the path is early blocke at LOCAL_IN or LOCAL_OUT

	//Used to the current controller update the border status of neighborhod clusters allowing to properly select (sincronized way) the entry and exit point
	if (coordinator_address != cluster_addr){ //It is not necessary the source controller do that since it execute the update border protocol
		update_cluster_border(selected_subnet, msg[7], &msg[8]);
	}

	source_x = source >> 8;
	source_y = source & 0xFF;
	target_x = target >> 8;
	target_y = target & 0xFF;

	//Define os limites X e Y em relacao ao target
	ref_x = target_x;
	ref_y = target_y;

#if SDN_DEBUG
	Puts("\nSource: "); Puts(itoa(source_x)); Puts("x");Puts(itoa(source_y)); Puts("    target: "); Puts(itoa(target_x)); Puts("x");Puts(itoa(target_y)); Puts("\n");
	
	//printf("\n------------- Cluster %dx%d received detailed routing request: ", x_cluster_addr, y_cluster_addr);
	//printf("\n\t- Coordinator address: %d", coordinator_address);
	//Puts("\n\t- entry_border:"); print_port(entry_border);
	//Puts("\n\t- exit border: "); print_port(exit_boder); Puts("\n");
	//Puts("\n\t- subnet: "); Puts(itoa(selected_subnet)); Puts("\n");
#endif

	//O coordenador so envia o lado, aqui eh necessario definir qual serão EXATAMENTE os entry e exit point,
	//Se for LOCAL entao a coordenada é interna
	//Caso contrario deve-se executar
	//Define o entry point caso este nao seja um roteador local do cluster
	//O entry point objetiva calcular somente o valor de uma coordenada, ou source_x, ou sorce_y, dependendo da borda de entrada
	if (entry_border != LOCAL_IN){

		switch (entry_border) {
			case EAST:
				source_x = x_offset + SDN_XCLUSTER-1;
				source_y = -1;
				input_status = cluster_border_output[x_cluster_addr+1][y_cluster_addr][selected_subnet][WEST] | cluster_border_input[x_cluster_addr][y_cluster_addr][selected_subnet][EAST];
				break;
			case WEST:
				source_x = x_offset;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				source_y = -1;
				input_status = cluster_border_output[x_cluster_addr-1][y_cluster_addr][selected_subnet][EAST] | cluster_border_input[x_cluster_addr][y_cluster_addr][selected_subnet][WEST];
				break;
			case NORTH:
				source_x = -1;//Recebe -1 pois sera calculado pelo algoritmo abaixo
				source_y = y_offset + SDN_YCLUSTER-1;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				input_status = cluster_border_output[x_cluster_addr][y_cluster_addr+1][selected_subnet][SOUTH] | cluster_border_input[x_cluster_addr][y_cluster_addr][selected_subnet][NORTH];
				break;
			case SOUTH:
				source_x = -1;//Recebe -1 pois sera calculado pelo algoritmo abaixo
				source_y = y_offset;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				input_status = cluster_border_output[x_cluster_addr][y_cluster_addr-1][selected_subnet][NORTH] | cluster_border_input[x_cluster_addr][y_cluster_addr][selected_subnet][SOUTH];
				break;
		}

		//Após definir a coordenada estatica (que eh independente do status dos roteadores poir representa a borda)
		//Somente salva o status da borda de entrada com base na borda de entrada;
		//input_status = cluster_border_input[x_cluster_addr][y_cluster_addr][entry_border];
#if SDN_DEBUG
		Puts("\nInput status: "); Puts(itoa(input_status));
#endif

		//Testa se a borda de entrada esta na posicao horizontal
		if (entry_border == NORTH || entry_border == SOUTH){

			//printf("\nEntry border is NORTH or SOUTH");
			heuristic(SDN_XCLUSTER, x_offset, input_status, &source_x, ref_x);

		} else { // Se a borda de entrada e LESTE OU OESTE

			//printf("\nEntry border is LEST or OESTE");
			//printf("\nMin_y: %d", min_y);
			//printf("\nMax_y: %d", max_y);
			heuristic(SDN_YCLUSTER, y_offset, input_status, &source_y, ref_y);

		}

	} /*else if (cs_inport[source_x-x_offset][source_y-y_offset][subnet_selector][LOCAL_IN] != -1){ // else the entry_border is LOCAL_IN then test the LOCAL_IN input
		detailed_routing_ret = 0;
	}*/

	//printf("X_cluster: %d    y_cluster: %d",x_cluster_addr, y_cluster_addr);
	//Repete o mesmo processo de cima porem escolhendo a saida
	if (exit_boder != LOCAL_OUT){

		switch (exit_boder) {
			case EAST:
				output_status = cluster_border_output[x_cluster_addr][y_cluster_addr][selected_subnet][EAST] | cluster_border_input[x_cluster_addr+1][y_cluster_addr][selected_subnet][WEST];
				target_x = x_offset + SDN_XCLUSTER-1;
				target_y = -1;
				break;
			case WEST:
				output_status = cluster_border_output[x_cluster_addr][y_cluster_addr][selected_subnet][WEST] | cluster_border_input[x_cluster_addr-1][y_cluster_addr][selected_subnet][EAST];
				target_x = x_offset;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				target_y = -1;
				break;
			case NORTH:
				output_status = cluster_border_output[x_cluster_addr][y_cluster_addr][selected_subnet][NORTH] | cluster_border_input[x_cluster_addr][y_cluster_addr+1][selected_subnet][SOUTH];
				target_x = -1;//Recebe -1 pois sera calculado pelo algoritmo abaixo
				target_y = y_offset + SDN_YCLUSTER -1;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				break;
			case SOUTH:
				output_status = cluster_border_output[x_cluster_addr][y_cluster_addr][selected_subnet][SOUTH] | cluster_border_input[x_cluster_addr][y_cluster_addr-1][selected_subnet][NORTH];
				target_x = -1;//Recebe -1 pois sera calculado pelo algoritmo abaixo
				target_y = y_offset;//Como o ponto de entrada ira varia no eixo X, o eixo Y ja pode ser definido
				break;

		}

#if SDN_DEBUG
		Puts("\nOutput status: "); Puts(itoa(output_status));
#endif
		
		//Testa se a borda de entrada esta na posicao horizontal
		if (exit_boder == NORTH || exit_boder == SOUTH){

			//printf("\n Exit border is NORTH or SOUTH");

			heuristic(SDN_XCLUSTER, x_offset, output_status, &target_x, ref_x);

		} else { // Se a borda de entrada e LESTE OU OESTE

			//printf("\n Exit border is LEST or OESTE");
			//printf("\nMin_y: %d", min_y);
			//printf("\nMax_y: %d", max_y);
			heuristic(SDN_YCLUSTER, y_offset, output_status, &target_y, ref_y);

		}

	}/* else if (cs_inport[target_x-x_offset][target_y-y_offset][subnet_selector][LOCAL_OUT] != -1){ // else the entry_border is LOCAL_IN then test the LOCAL_IN input
		detailed_routing_ret = 0;
	}*/

	//Neste ponto os roteadores do entry e exit point ja foram definidos e pode-se aplicar o hadlock local para ver se ha caminho

#if SDN_DEBUG
	Puts("\n---- Cluster "); Puts(itoa(x_cluster_addr)); Puts("x"); Puts(itoa(y_cluster_addr)); Puts("-----: ");
	Puts("\nEntry point defined at:"); Puts(itoa(source_x)); Puts("x"); Puts(itoa(source_y));
	Puts("\nExit point defined at:"); Puts(itoa(target_x)); Puts("x"); Puts(itoa(target_y)); Puts("\n");
#endif
	
	enable_local_routing();

	source_x = source_x - x_offset;
	source_y = source_y - y_offset;
	target_x = target_x - x_offset;
	target_y = target_y - y_offset;

	//printf("\nInternal Entry point defined at: %dx%d", source_x, source_y);
	//("\nInternal Exit point defined at : %dx%d\n", target_x, target_y);
	detailed_routing_ret = 1;

	if ( (entry_border == LOCAL_IN && cs_inport[source_x][source_y][selected_subnet][LOCAL_IN] != -1) || (exit_boder == LOCAL_OUT && cs_inport[target_x][target_y][selected_subnet][LOCAL_OUT] != -1)){
		detailed_routing_ret = 0;
#if SDN_DEBUG
		Puts("\n EARLY BLOKED true\n");
#endif
	}

	if (detailed_routing_ret)
		detailed_routing_ret = hadlock(source_x, source_y, target_x, target_y, selected_subnet, 0); //reuse of variable min_x

#if SDN_DEBUG
	Puts("Hadlock path found: "); Puts(itoa(detailed_routing_ret)); Puts("\n\n");
	Puts("Send detailed routing response\n");
#endif
	
	if(coordinator_address != cluster_addr){
		//If the function is called from the perspective of others controllers, sends the response to the coordinator
		send_detailed_routing_response(detailed_routing_ret, coordinator_address, entry_border, (source_x << 8 | source_y));

	} else {
		//If it is the coordinator them call itself the function to handle the detailed routing response
		handle_detailed_routing_response(detailed_routing_ret, coordinator_address, entry_border, (source_x << 8 | source_y));

	}

}

unsigned short int compute_path_utilization(int controller_addr_x, int controller_addr_y){
	unsigned short int path_util = 0;

	for(int s=0; s<CS_NETS; s++){
		path_util += global_subnet_utilization[controller_addr_x][controller_addr_y][s];
	}

	return path_util;
}

int compute_subnet_utilization_of_path(int subnet){

	int curr_x, curr_y, total_util;

	total_util = 0;

	for(int i=0; i<global_path_size; i++){

		curr_x = (cluster_path[i] & 0xFF000000) >> 24;//Extracts the cluster X address
		curr_y = (cluster_path[i] & 0x00FF0000) >> 16;//Extracts the cluster Y address

		total_util += global_subnet_utilization[curr_x][curr_y][subnet];
	}

	return total_util;
}

void global_routing(){

	//1.Discover source cluster
	//2.Discover target cluster
	//3.Employ hadlock to achieve the path
	//4.Get the path and search the entry and exit points
	//5.Sends the entry and exit points to the correponding clusters
	unsigned int source_cluster_x, source_cluster_y, target_cluster_x, target_cluster_y;
	unsigned int curr_x, curr_y, inport, outport;
	unsigned int clust_addr_aux, hadlock_ret, index;
	int lower_util, curr_util, curr_path_size;
	unsigned int local_message[MAX_MANAG_MSG_SIZE];//message is the array used to send to anothers controllers, local_message is the array used to execute the detailed_cluster algorithm
	unsigned int * message;

	//1. Discover source cluster
	source_cluster_x = (global_path_request.source >> 8) / SDN_XCLUSTER;
	source_cluster_y = (global_path_request.source & 0xFF) / SDN_YCLUSTER;

	//2.Discover target cluster
	target_cluster_x = (global_path_request.target >> 8) / SDN_XCLUSTER;
	target_cluster_y = (global_path_request.target & 0xFF) / SDN_YCLUSTER;
	//cluster_id = aux_x + (aux_y * SDN_X_CLUSTER_NUM);
#if SDN_DEBUG
	Puts("\n------------------ GLOBAL ROUTING ------------------- \n");
	Puts("\nCoordinator "); Puts(itoa(cluster_addr)); Puts(" starting GLOBAL ROUTING \nSource "); Puts(itoa((global_path_request.source >> 8))); Puts("x");
	Puts(itoa((global_path_request.source & 0xFF))); Puts(" target "); Puts(itoa((global_path_request.target >> 8))); Puts("x"); Puts(itoa((global_path_request.target & 0xFF))); Puts("\n");

	Puts("Source cluster: "); Puts(itoa(source_cluster_x)); Puts("x"); Puts(itoa(source_cluster_y)); Puts("\n");
	Puts("Target cluster: "); Puts(itoa(target_cluster_x)); Puts("x"); Puts(itoa(target_cluster_y)); Puts("\n");


	/*Global Routing heuristic-----------------------------------------------*/
	Puts("\nGlobal routing result:");
#endif
	//3.Employ hadlock to achieve the path
	//Here also should be a heuristc to select the less congested subnet
	enable_global_routing();

	//1. Compute the path utilization of each cluster
	//3. Applies hadlock all subnets in minial path mode. Stores each sucessifull subnet
	//5. If fail: Applies hadlock for all subnets in non minial. Stores each successifull subnet

	//In case of success:
	//1. Select the lower utilization subnet

	//Minpath enabled
	lower_util = -1;
	curr_path_size = 65535;
	global_path_request.subnet = -1;
	//Loop that executes two times: the first one is with minimal path enabled, the second one is with minimal path disabled
	for(int min_path=1; min_path >= 0 && global_path_request.subnet == -1; min_path--){

		//Forces the search to shortest path when the number of global routing attemps is higher than 1
		if (global_routing_attempts > 1)
			min_path = 0;

		//For each subnet
#if SDN_DEBUG
		//Puts("\nTrying with mininal path "); Puts(itoa(min_path)); Puts("\n");
#endif
		for(int subnet = 0; subnet < CS_NETS; subnet++){

#if SDN_DEBUG
			Puts("\nTrying subnet "); Puts(itoa(subnet)); Puts("\n");
#endif
			//Aplies hadlock
			hadlock_ret = hadlock(source_cluster_x, source_cluster_y, target_cluster_x, target_cluster_y, subnet, min_path);
			if (hadlock_ret){
				//If hadlock succeed, retrace the path to discovery the controller addresses
				retrace(subnet, LOCAL_IN, LOCAL_OUT);
				//Compute the subnet utilization for that path and for the subnet
				curr_util = compute_subnet_utilization_of_path(subnet);
#if SDN_DEBUG
				//Puts("Hadlock success, utilization of subnet is "); Puts(itoa(curr_util)); Puts("\n");
#endif
				//Sets the lower subnet as the choosed one

				//Priorizes the utlization as factor to decide the best subnet
				//If utilizatin is the same as the lower one, verify if the path size is lower
				if ( (lower_util == -1 || curr_util < lower_util) || (curr_util == lower_util && global_path_size < curr_path_size) ){
#if SDN_DEBUG
					Puts("\nSubent defined\n");
#endif
					global_path_request.subnet = subnet; //Index is reused only to keep the informatio that at least one subnet works
					lower_util = curr_util;
					curr_path_size = global_path_size;
				}
			} 
#if SDN_DEBUG
			else
				Puts("Hadlock failure\n");
#endif
		}

		//Avoid reexecution since the minimal path flag was disabled previosly
		if (global_routing_attempts > 1)
			break;
	}

	if (lower_util == -1){
#if SDN_DEBUG
		Puts("\n\nGlobal path TOTAL failure\n\n");
#endif
		send_global_mode_release(0);
		//send_ack_requester(-1, global_path_request.source, global_path_request.target, global_path_request.requester_address, 1, ESTABLISH);
		//send_token_release();

		return;
	}

	if (global_path_request.subnet > -1){
#if SDN_DEBUG
		Puts("\nSubnet found: "); Puts(itoa(global_path_request.subnet)); Puts(" utilizaiton "); Puts(itoa(lower_util)); Puts(" path size "); Puts(itoa(global_path_size)); Puts("\n");
#endif
		hadlock(source_cluster_x, source_cluster_y, target_cluster_x, target_cluster_y, global_path_request.subnet, 0);
		retrace(global_path_request.subnet, LOCAL_IN, LOCAL_OUT);
		//print_input_borders(global_path_request.subnet);
		//print_output_borders(global_path_request.subnet);
		//print_subnet_utilization(global_path_request.subnet);
	} else {
		Puts("\n\n\nERROR: Not possible to do global routing!\n\n\n");
	}

	/*EndGlobal Routing heuristic-----------------------------------------------*/


	
	
	//Para cada cluster verifica qual foi a borda de entrada e a borda de saida
	for(int i=global_path_size-1; i>=0; i--){

		curr_x = (cluster_path[i] & 0xFF000000) >> 24;//Extracts the cluster X address
		curr_y = (cluster_path[i] & 0x00FF0000) >> 16;//Extracts the cluster Y address
		inport = (cluster_path[i] & 0x0000FF00) >> 8;//Extracts the inport
		outport = cluster_path[i] & 0x000000FF;//Extracts the outport

		clust_addr_aux = curr_x << 8 | curr_y; //Composes the cluster address

		if (clust_addr_aux != cluster_addr){

			/*Fills message with the updated cluster border status*/
			message = get_message_slot();
			message[0] = DETAILED_ROUTING_REQUEST;
			message[1] = cluster_addr;
			message[2] = global_path_request.source;
			message[3] = global_path_request.target;
			message[4] = global_path_request.subnet;
			message[5] = inport; //Entry point
			message[6] = outport; //Exit point
			message[7] = global_path_size;
			index = 8;

			for(int i=0; i<global_path_size; i++){

				curr_x = (cluster_path[i] & 0xFF000000) >> 24;//Extracts the cluster X address
				curr_y = (cluster_path[i] & 0x00FF0000) >> 16;//Extracts the cluster Y address

				message[index++] = curr_x << 8 | curr_y; //Cluster address X'
				for(int k=0; k<4; k++){ //4 is the number of border in each cluster
					message[index++] = cluster_border_input[curr_x][curr_y][global_path_request.subnet][k];
					message[index++] = cluster_border_output[curr_x][curr_y][global_path_request.subnet][k];
				}
			}
			/*End filling message*/
			
			send(clust_addr_aux, message, index);

#if SDN_DEBUG
			Puts(" Message DETAILED_ROUTING_REQUEST sent to cluster "); Puts(itoa(clust_addr_aux)); Puts(" ("); Puts(itoa(curr_x)); Puts("x"); Puts(itoa(curr_y)); Puts("\n");
			Puts(" - "); print_port(inport); Puts(" -> "); print_port(outport); Puts("\n");
#endif
			
		} else {
			//Here the own cluster must handle the request
			local_message[0] = DETAILED_ROUTING_REQUEST;
			local_message[1] = cluster_addr;
			local_message[2] = global_path_request.source;
			local_message[3] = global_path_request.target;
			local_message[4] = global_path_request.subnet; //Entry point
			local_message[5] = inport; //Entry point
			local_message[6] = outport; //Exit point

		}

	}

	controllers_response_counter = 0;
	//Coordinator controller handles its own cluster path
	detailed_routing(local_message);

}

void update_cluster_border(int subnet, int num_controller, unsigned int *msg){

	int controller_addr, x_addr, y_addr;
	unsigned short int status;

/*#if SDN_DEBUG
	Puts("Update cluster border ---- num_controller: "); Puts(itoa(num_controller)); Puts(" subnet["); Puts(itoa(subnet)); Puts("]\n");
#endif*/
	
	for(int i=0; i<num_controller; i++){

		controller_addr = *msg++; //Cluster address X'
		x_addr = controller_addr >> 8;
		y_addr = controller_addr & 0xFF;
		for(int k=0; k<4; k++){ //4 is the number of border in each cluster

			status = (unsigned short int) *msg++;
			cluster_border_input[x_addr][y_addr][subnet][k] = status;
			status = (unsigned short int) *msg++;
			cluster_border_output[x_addr][y_addr][subnet][k] = status;
		}
	}
/*#if SDN_DEBUG
	Puts("Update cluster boraders\n");
#endif*/
	//print_input_borders(subnet);
	//print_output_borders(subnet);
}


//|---------|  0
//|---------|  |
//|---------| \|/
//|---------| 32
//32 <----- 0

void build_border_status(){
	int x,y, nc_x, nc_y;

	nc_x = cluster_addr >> 8;
	nc_y = cluster_addr & 0xFF;

#if SDN_DEBUG
	//Puts("\nBuilding border status....\n");
#endif
	//Look at each border
	for (int subnet=0; subnet<CS_NETS; subnet++){
		for(int border = 0; border < 4; border++){

			cluster_border_input[nc_x][nc_y][subnet][border] = 0;
			cluster_border_output[nc_x][nc_y][subnet][border] = 0;

			switch (border) {
				case EAST:
					x = SDN_XCLUSTER-1;
					for(int i=0; i < SDN_YCLUSTER; i++){
						y = (SDN_YCLUSTER-1)-i;
						cluster_border_input[nc_x][nc_y][subnet][border] |= !is_PE_propagable(x, y, EAST, subnet) << i;
						cluster_border_output[nc_x][nc_y][subnet][border] |= !is_PE_reachable(x, y, subnet) << i;
					}
					break;
				case WEST:
					x = 0;
					for(int i=0; i < SDN_YCLUSTER; i++){
						y = (SDN_YCLUSTER-1)-i;
						cluster_border_input[nc_x][nc_y][subnet][border] |= !is_PE_propagable(x, y, WEST, subnet) << i;
						cluster_border_output[nc_x][nc_y][subnet][border] |= !is_PE_reachable(x, y, subnet) << i;
					}
					break;
				case NORTH:
					y = SDN_YCLUSTER-1;
					for(int i=0; i < SDN_XCLUSTER; i++){
						x = (SDN_XCLUSTER-1)-i;
						cluster_border_input[nc_x][nc_y][subnet][border] |= !is_PE_propagable(x, y, NORTH, subnet) << i;
						cluster_border_output[nc_x][nc_y][subnet][border] |= !is_PE_reachable(x, y, subnet) << i;
					}
					break;
				case SOUTH:
					y = 0;
					for(int i=0; i < SDN_XCLUSTER; i++){
						x = (SDN_XCLUSTER-1)-i;
						cluster_border_input[nc_x][nc_y][subnet][border] |= !is_PE_propagable(x, y, SOUTH, subnet) << i;
						cluster_border_output[nc_x][nc_y][subnet][border] |= !is_PE_reachable(x, y, subnet) << i;
					}
					break;
			}
		}
	}

}

void handle_global_mode_release(unsigned int * rcv_msg){
	int msg_size;
	unsigned int path_success, coordinator_addr, inport, outport, subnet;
	unsigned int * message;

	path_success 		= rcv_msg[1];
	coordinator_addr 	= rcv_msg[2];

#if SDN_DEBUG
	Puts("Receiving GLOBAL_MODE_RELEASE from coordinator "); Puts(itoa(coordinator_addr)); Puts("\n");
#endif

	message = get_message_slot();
	message[0] = GLOBAL_MODE_RELEASE_ACK;
	message[1] = cluster_addr;
	message[2] = 0;
	msg_size = 3;

	if (path_success){
		inport 	= rcv_msg[3];
		outport = rcv_msg[4];
		subnet 	= rcv_msg[5];

#if SDN_DEBUG
		Puts("Start reconfiguring path\n");
		Puts("\n***************retrace*******************");
#endif

		retrace(subnet, inport, outport);

#if SDN_DEBUG
		Puts("*******************************************\n");
#endif

		//Edit the message with the path size
		message[2] = print_path_size;

#if PATH_DEBUG
		//Add to the messsage the path size
		for(int i=0; i<print_path_size; i++){
			message[msg_size++] = print_paths[i];
		}
#endif

	}

	send(coordinator_addr, message, msg_size);

#if SDN_DEBUG
	Puts("\nMessage GLOBAL_MODE_RELEASE_ACK sent to coordinator \n");
#endif
}


void handle_update_border_ack(unsigned int * msg){

	int x_addr, y_addr, msg_index, cluster_index;


	x_addr = msg[1] >> 8;
	y_addr = msg[1] & 0xFF;
	cluster_index = msg[2];

#if SDN_DEBUG
	Puts("Message UPDATE_BORDER_ACK received from cluster "); Puts(itoa(x_addr)); Puts("x"); Puts(itoa(y_addr)); Puts(" - index "); Puts(itoa(cluster_index)); Puts("\n");
#endif

	msg_index = 3;

	/*Network utilization statistics ---------------------------------------*/
	//Execute on the first time this function is called
	//Initializes the global with the values of the coordinator cluster
	for(int s=0; s<CS_NETS; s++){
		global_subnet_utilization[x_addr][y_addr][s] = msg[msg_index++];
/*#if SDN_DEBUG
		Puts("subnet util: "); Puts(itoa(global_subnet_utilization[x_addr][y_addr][s])); Puts("\n");
#endif*/
	}
	/*End Network utilization statistics ---------------------------------------*/
	for(int s=0; s<CS_NETS; s++){
		for(int i=0; i<4; i++){
			cluster_border_input[x_addr][y_addr][s][i] = msg[msg_index++];
			cluster_border_output[x_addr][y_addr][s][i] = msg[msg_index++];
		}
	}

	controllers_response_counter++;

#if SDN_DEBUG
	Puts("Ack received = "); Puts(itoa(controllers_response_counter)); Puts(" - Total messages = "); Puts(itoa(NC_NUMBER)); Puts("\n");
#endif
	
	if (controllers_response_counter == NC_NUMBER){
#if SDN_DEBUG
		Puts("All border status received\n");
#endif

		for(int s=0; s<CS_NETS; s++){//Set itss own utilization
			global_subnet_utilization[x_cluster_addr][y_cluster_addr][s] += subnet_utilization[s];
		}

#if SDN_DEBUG
		Puts("Starting global routing\n");
#endif

		for(int x=0; x<SDN_X_CLUSTER_NUM; x++){
			for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){
				for(int s=0; s<CS_NETS; s++){
					available_controllers[x][y][s] = 1;
				}
			}
		}

		global_routing_attempts = 1;

		global_routing();

	}

}



void handle_NI_status_request(unsigned int targetPE, unsigned int req_address){

	unsigned int conn_in, conn_out, tx, ty;
	unsigned int * message = get_message_slot();

	conn_in = 0;
	conn_out = 0;
	tx = targetPE >> 8;
	ty = targetPE & 0xFF;

	for(int i=0; i<CS_NETS; i++){

		if ( cs_inport[tx][ty][i][LOCAL_IN] == 0 )
			conn_out++;

		if ( cs_inport[tx][ty][i][LOCAL_OUT] == 0 )
			conn_in++;
	}


	message[0] = NI_STATUS_RESPONSE;
	message[1] = conn_in;
	message[2] = conn_out;
	send(req_address, message, 3);
}

void initialize_noc_manager(unsigned int * msg){
	unsigned int max_ma_tasks;
	unsigned int task_id, proc_addr, id_offset;

	Puts("\n ************ Initialize NoC manager *********** \n");
	max_ma_tasks = msg[3];

	//Set the proper location of all MA tasks
	for(int i=0; i<max_ma_tasks; i++){
		task_id = msg[i+4] >> 16;
		proc_addr = msg[i+4] & 0xFFFF;
		//Puts("Task MA "); Puts(itoa(task_id)); Puts(" allocated at "); Puts(itoh(proc_addr)); Puts("\n");
		AddTaskLocation(task_id, proc_addr);
	}

	task_id = msg[1];
	id_offset = msg[2];
	/*Puts("Task ID: "); Puts(itoa(task_id)); Puts("\n");
	Puts("Offset ID: "); Puts(itoa(id_offset)); Puts("\n");
	Puts("MAX_MA_TASKS: "); Puts(itoa(msg[3])); Puts("\n");*/
	//Puts("Cluster addr: "); Puts(itoh(conv_task_ID_to_cluster_addr(task_id, id_offset, SDN_X_CLUSTER_NUM))); Puts("\n");

	cluster_id = task_id - id_offset;
	//Initialize things related to SDN management
	init_cluster_address_offset(cluster_id);

	init_generic_send_comm(id_offset, SDN_XCLUSTER);

	Puts("NoC Manager initialized!!!!!!\n\n");
}


int handle_packet(unsigned int * recv_message){

	switch (recv_message[0]) {
		case PATH_CONNECTION_REQUEST:
			handle_component_request(recv_message[1], recv_message[2], recv_message[3], -1);
			break;
		case PATH_CONNECTION_RELEASE:
			handle_component_request(recv_message[1], recv_message[2], recv_message[3], recv_message[4]);
			break;
		case NI_STATUS_REQUEST:
			handle_NI_status_request(recv_message[1], recv_message[2]);
			break;
		case DETAILED_ROUTING_REQUEST:
			detailed_routing(recv_message);
			break;
		case DETAILED_ROUTING_RESPONSE:
			handle_detailed_routing_response(recv_message[1], recv_message[2], recv_message[3], recv_message[4]);
			break;
		case TOKEN_REQUEST:
			handle_token_request(recv_message[1]);
			break;
		case TOKEN_GRANT:
			handle_token_grant();
			break;
		case TOKEN_RELEASE:
			handle_token_release();
			break;
		case UPDATE_BORDER_REQUEST:
			handle_update_border_request(recv_message[1]);

			if(controller_status == IDLE){
				controller_status = GLOBAL_SLAVE;
#if SDN_DEBUG
				Puts("Controller is GLOBAL_SLAVE\n");
#endif
			} else {
				Puts("ERROR: controller should be in idle UPDATE_BORDER_REQUEST\n");
				while(1);
			}

			break;
		case UPDATE_BORDER_ACK:
			handle_update_border_ack(recv_message);
			break;
		case LOCAL_RELEASE_REQUEST:
			global_path_release(recv_message[1], recv_message[2], recv_message[3], recv_message[4], recv_message[5], recv_message[6]);
			break;
		case LOCAL_RELEASE_ACK:
			handle_local_release_ack(recv_message[1], recv_message[2], recv_message[3]);
			break;
		case GLOBAL_MODE_RELEASE:
			
			handle_global_mode_release(recv_message);

			if (controller_status == GLOBAL_SLAVE){
				controller_status = IDLE;
#if SDN_DEBUG
				Puts("Controller IDLE 3\n");
#endif
			} else {
				Puts("ERROR: controller is not in GLOBAL_SLAVE\n");
				while(1);
			}
			break;
		case GLOBAL_MODE_RELEASE_ACK:
			handle_global_mode_release_ack(recv_message);
			break;
		case INITIALIZE_MA_TASK:
			initialize_noc_manager(recv_message);
			break;
		default:
			Puts("ERROR message not indentified\n");
			while(1);
			break;
	}
	return 1;

}

/**Computes if the current source and target PE addresses are within the controller region
 *
 */
int is_local_connection(int source_x, int source_y, int target_x, int target_y){

	//If this condition is true, them the PE is within the controller region
	if (	source_x < x_offset 				||
			source_x >= (x_offset + SDN_XCLUSTER) 	||
			source_y < y_offset					||
			source_y >= (y_offset + SDN_YCLUSTER)	||
			target_x < x_offset 				||
			target_x >= (x_offset + SDN_XCLUSTER) 	||
			target_y < y_offset					||
			target_y >= (y_offset + SDN_YCLUSTER)){
		return 0;
	}

	return 1;
}

void handle_local_release_ack(unsigned int source_controller, int controllers_nmbr, int is_last_controller){

	controllers_response_counter++;

#if SDN_DEBUG
	Puts("Message LOCAL_RELEASE_ACK received from "); Puts(itoa(source_controller)); Puts("\n");
	//printf("Entry border "); print_port(entry_border);
	//printf("\nEntry router %dx%d\n", (entry_router >> 8), entry_router & 0xFF);
	Puts("Controllers number: "); Puts(itoa(controllers_nmbr)); Puts("\n");
#endif


	//global_path_size
	if (is_last_controller == 1)
		global_path_size = controllers_nmbr;

	if (controllers_response_counter == global_path_size){

#if SDN_DEBUG
		Puts("\n\n\t\tEND RELEASE PROCESS\n\n");
#endif

		send_ack_requester(global_path_request.subnet, global_path_request.source, global_path_request.target, global_path_request.requester_address, 1, RELEASE);

		send_token_release();
	}

}

void global_path_release(int inport, int source, int target, int controller_number, int coordinator_addr, int mp){

	int limit_addr, x, y, next_source_x, next_source_y, next_nc_x, next_nc_y;
	int src_x, src_y, tgt_x, tgt_y;
	char is_allocated;
	unsigned int * message = get_message_slot();

	//Set to zero, it will be used in the handle_local_release_ack to check the number of controllers response
	global_path_size = 0;

	src_x = (source >> 8) - x_offset;
	src_y = (source & 0xFF) - y_offset;
	tgt_x = (target >> 8) - x_offset;
	tgt_y = (target & 0xFF) - y_offset;

	is_allocated = cs_inport[src_x][src_y][mp][inport];

#if SDN_DEBUG
	Puts("\n----------Cluster"); Puts(itoa(cluster_addr)); Puts(" received GLOBAL_PATH_RELEASE------------\n");
	Puts("Controller nr "); Puts(itoa(controller_number)); Puts(" source "); Puts(itoa((source >> 8))); Puts("x"); Puts(itoa((source & 0xFF))); Puts("  target "); Puts(itoa((target >> 8)));
	Puts("x"); Puts(itoa((target & 0xFF))); Puts(" inport  subnet["); Puts(itoa(mp)); Puts("] \n");
	print_port(inport); Puts("\n");


	//print_router_status(mp);
	
#endif


	if (is_allocated == -1){
		Puts("ERROR, input port doesn't have an path id\n");
		while(1);

	}

	limit_addr = CS_connection_release(src_x, src_y, tgt_x, tgt_y, mp, inport);

	//Esse codigo a seguir pega o endereço limite alcancado pelo algoritmo de release e procura qual eh a borda de saida
	//pra poder saber pra qual controlador a proxima mensagem deve ser enviada
	if (limit_addr != -1){


		inport = limit_addr & 0x0000FFFF;
		limit_addr = limit_addr >> 16;
		x = limit_addr >> 8;
		y = limit_addr & 0xFF;

		next_nc_x = x_cluster_addr;
		next_nc_y = y_cluster_addr;

		next_source_x = x + x_offset;
		next_source_y = y + y_offset;

#if SDN_DEBUG
		Puts("\n\nLimite do cluster alcancado, router "); Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts("\n");
#endif
		
		switch (inport) {
			case EAST:
				next_source_x--;
				next_nc_x--;
#if SDN_DEBUG
				Puts("Saida OESTE\n");
#endif
				break;
			case WEST:
				next_source_x++;
				next_nc_x++;
#if SDN_DEBUG
				Puts("Saida LESTE\n");
#endif
				break;
			case NORTH:
				next_source_y--;
				next_nc_y--;
#if SDN_DEBUG
				Puts("Saida SUL\n");
#endif
				break;
			case SOUTH:
				next_source_y++;
				next_nc_y++;
#if SDN_DEBUG
				Puts("Saida NORTE\n");
#endif
				break;
		}

#if SDN_DEBUG
		Puts("Next router address: "); Puts(itoa(next_source_x)); Puts("x"); Puts(itoa(next_source_y)); Puts(" at cluster "); Puts(itoa(next_nc_x)); Puts("x"); Puts(itoa(next_nc_y)); Puts("\n");
#endif
		
		//Send the message to the next cluster
		message[0] = LOCAL_RELEASE_REQUEST;
		message[1] = inport;
		message[2] = (next_source_x << 8 | next_source_y);
		message[3] = target;
		message[4] = controller_number + 1;
		message[5] = coordinator_addr;
		message[6] = mp;
		send((next_nc_x << 8 | next_nc_y), message, 7);

#if SDN_DEBUG
		Puts("Send LOCAL_RELEASE_REQUEST to cluster\n");
#endif

		message = get_message_slot();

		message[3] = 0; //If 1 means the end of allocation process

	} else {

#if SDN_DEBUG
		Puts("End of the alocation\n");
#endif

		message[3] = 1; //If 1 means the end of allocation process
	}
#if SDN_DEBUG
	//print_router_status(mp);
#endif

	//Build the border status and sends the LOCAL_RELEASE_ACK
	if (coordinator_addr != cluster_addr){

#if SDN_DEBUG
		Puts("Sending message LOCAL_RELEASE_ACK to coordinator "); Puts(itoa(coordinator_addr)); Puts("\n");
#endif
		
		message[0] = LOCAL_RELEASE_ACK;
		message[1] = cluster_addr;
		message[2] = controller_number;
		//message[3] was already defined

		send(coordinator_addr, message, 3);
	}

}


/*#################################### NEW MODULAR IMPLEMENTAITON ###############################################################################*/

/** This function handle all requests sent from any component that wants to establishes or release a connection
 * \param sourcePE: Source PE address
 * \param targetPE: Target PE address
 * \param requester_address: Rrquester components address
 * \param mode: If this value is 0 the connection request is ESTABLISHMENT otherwise is RELEASE
 */
void handle_component_request(int sourcePE, int targetPE, int requester_address, int subnet){
	int source_x, source_y, target_x, target_y;

	source_x = sourcePE >> 8;
	source_y = sourcePE & 0xFF;

	target_x = targetPE >> 8;
	target_y = targetPE & 0xFF;

#if SDN_DEBUG
	Puts("\n****** New request received at "); Puts(itoa(x_cluster_addr)); Puts("x"); Puts(itoa(y_cluster_addr)); Puts(", source "); Puts(itoa(source_x)); Puts("x"); Puts(itoa(source_y)); Puts(", target "); Puts(itoa(target_x)); Puts("x"); Puts(itoa(target_y));
	Puts(", requester_addr "); Puts(itoa(requester_address)); Puts(", Subnet "); Puts(itoa(subnet)); Puts("\n");
#endif
	//Adds to the respective queue to be handled in the future
	if (is_local_connection(source_x, source_y, target_x, target_y)) {
		add_local_connection_request(sourcePE, targetPE, requester_address, subnet);
#if SDN_DEBUG
		Puts("Local connection\n");
#endif
	} else {
		add_global_connection_request(sourcePE, targetPE, requester_address, subnet);
#if SDN_DEBUG
		Puts("Global connection\n");
#endif
	}


}


void new_global_path(ConnectionRequest * conn_request_ptr){
	//Stores source and target to be used when token is granted

	global_path_request.source = conn_request_ptr->source;
	global_path_request.target = conn_request_ptr->target;
	global_path_request.requester_address = conn_request_ptr->requester_address;
	global_path_request.subnet = conn_request_ptr->subnet;
	
#if SDN_DEBUG
	Puts("\n###########################################\nStarting new GLOBAL path request\n###########################################\n");
	Puts("Communication pairs: ");
	Puts(itoa(global_path_request.source >> 8)); Puts("x"); Puts(itoa(global_path_request.source & 0xFF)); Puts(" -> ");
	Puts(itoa(global_path_request.target >> 8)); Puts("x"); Puts(itoa(global_path_request.target & 0xFF));
	Puts("\n");
#endif

	//send token request and wait token release
	send_token_request();

}

void new_local_path(ConnectionRequest * conn_request_ptr){

	int source_x, source_y, target_x, target_y, connection_ret;

#if SDN_DEBUG
	Puts("\n********************************************\nStarting new LOCAL path request\n****************************************\n");
	Puts("Communication pairs: ");
	Puts(itoa(conn_request_ptr->source >> 8)); Puts("x"); Puts(itoa(conn_request_ptr->source & 0xFF)); Puts(" -> ");
	Puts(itoa(conn_request_ptr->target >> 8)); Puts("x"); Puts(itoa(conn_request_ptr->target & 0xFF));
	Puts("\n");
#endif
	
	source_x = (conn_request_ptr->source >> 8) - x_offset;
	source_y = (conn_request_ptr->source & 0xFF) - y_offset;
	target_x = (conn_request_ptr->target >> 8) - x_offset;
	target_y = (conn_request_ptr->target & 0xFF) - y_offset;
	
	//Overhead for local paths
	path_overhead = GetTick();

	//Switches to local routing mode
	enable_local_routing();

	if (conn_request_ptr->subnet != -1) { //There is a valid path id then: release path

		connection_ret = CS_connection_release(source_x, source_y, target_x, target_y, conn_request_ptr->subnet, LOCAL_IN);

		if (connection_ret != -1){
			Puts("ERROR in Local connection release\n");
			while(1);
		}

		send_ack_requester(conn_request_ptr->subnet, conn_request_ptr->source, conn_request_ptr->target, conn_request_ptr->requester_address, 0, RELEASE);

	} else {

		connection_ret = CS_connection_setup(source_x, source_y, target_x, target_y);


#if PATH_DEBUG
		addPath(cluster_addr, print_paths, print_path_size);
#else
		printPathSize += print_path_size;
#endif
		send_ack_requester(connection_ret, conn_request_ptr->source, conn_request_ptr->target, conn_request_ptr->requester_address, 0, ESTABLISH);

	}
	
#if SDN_DEBUG
	//print_router_status(connection_ret);
	Puts("\n\n\tLOCAL PATH PROCESS FINISHEED\n\n");
#endif
}

/*###################################################################################################################*/

void init_cluster_address_offset(unsigned int id){
	unsigned int layers_above, cluster_per_layer;
	cluster_per_layer = (XDIMENSION / SDN_XCLUSTER);
	layers_above = id / cluster_per_layer;
	y_offset = layers_above * SDN_YCLUSTER;
	x_offset = (id - (cluster_per_layer * layers_above)) * SDN_XCLUSTER;

	x_cluster_addr = x_offset / SDN_XCLUSTER;//Representa a coordernada X do cluster
	y_cluster_addr = y_offset / SDN_YCLUSTER;//Representa a coordernada Y do cluster
	cluster_addr = x_cluster_addr << 8 | y_cluster_addr;
	Puts("XDIMENSION: "); Puts(itoa(XDIMENSION)); Puts("\n");
	Puts("YDIMENSION: "); Puts(itoa(YDIMENSION)); Puts("\n");
	Puts("SDN_XCLUSTER: "); Puts(itoa(SDN_XCLUSTER)); Puts("\n");
	Puts("SDN_YCLUSTER: "); Puts(itoa(SDN_YCLUSTER)); Puts("\n\n");

#if SDN_DEBUG
	Puts("CLuster offset = "); Puts(itoa(x_offset)); Puts("x"); Puts(itoa(y_offset)); Puts("\n");
	Puts("CLuster add = "); Puts(itoa(x_cluster_addr)); Puts("x"); Puts(itoa(y_cluster_addr)); Puts("\n");
	Puts("Cluster addr: "); Puts(itoa(cluster_addr)); Puts("\n");
	Puts("Cluster ID: "); Puts(itoa(id)); Puts("\n");
#endif
}

int main(){
	
	RequestServiceMode();
	//cluster_id = CLUSTER_ID;
    Puts("\n*************** NoC-Controller "); Puts(itoa(cluster_id)); Puts(" initialized! ******************\n");
    init_message_slots();
    initialize_MA_task();
    init_search_path();
    initit_cluster_borders();

    controller_status = IDLE;
#if SDN_DEBUG
    Puts("Controller IDLE\n");
#endif

    ConnectionRequest * conn_request = 0;

    //Data message
    unsigned int data_message[MAX_MANAG_MSG_SIZE];

    for(;;){

#if SDN_DEBUG
    	//Puts("\n....Waiting receive.....\n");
#endif
		ReceiveService(data_message);
		handle_packet(data_message);
    	
    	switch (controller_status) {
			case IDLE: //At IDLE search for global and local path request, as can be seen, the global path receive more priority since are checked first

				if (NoCSendFree()){
					
					conn_request = 0;
					//Only start a new global path if the token was not requested yet
					if (!token_requested){
						conn_request = get_next_global_connection_request();
						if (conn_request){
							new_global_path(conn_request);
						}
					} 
					
					if (!conn_request){
						conn_request = get_next_local_connection_request();
						if (conn_request){
							new_local_path(conn_request);
						}
					}
					
				}
				
				break;
			case GLOBAL_SLAVE: //Waiting token but can handle local path requests
			case GLOBAL_MASTER:
				//Controller is waiting messages from global_slaves
				ReceiveService(data_message);
				handle_packet(data_message);
				break;
		}

    }

    //exit();


}//end main





