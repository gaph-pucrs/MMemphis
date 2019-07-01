/*
 * noc_manager.h
 *
 *  Created on: 9 de out de 2018
 *      Author: mruaro
 */

#ifndef NOC_MANAGER_H_
#define NOC_MANAGER_H_

#include "../../software/include/management_api.h"

/** Used only to emulate a bigger many-core*/
#define XDIMENSION 6
#define YDIMENSION 6
#define XCLUSTER 3
#define YCLUSTER 3
#define SUBNETS_NUMBER 3
#define SPLIT 1
/***********************************************/

#define SDN_DEBUG 0
#define PATH_DEBUG 0

#define CLUSTER_X_NUMBER	(XDIMENSION/XCLUSTER)
#define CLUSTER_Y_NUMBER	(YDIMENSION/YCLUSTER)


#define NC_NUMBER		((XDIMENSION*YDIMENSION)/(XCLUSTER*YCLUSTER))

#define MAX_NC_PATHS		(2*XCLUSTER*YCLUSTER-XCLUSTER-YCLUSTER)

#define CS_NETS 		SUBNETS_NUMBER
#define ROUTER_NUMBER	(XCLUSTER*YCLUSTER)
#define PORT_NUMBER		6

#define EAST		0
#define WEST		1
#define NORTH		2
#define SOUTH		3
#define LOCAL_OUT	4
#define LOCAL_IN	5

unsigned int x_offset, y_offset;

char *itoa(unsigned int num)
{
   static char buf[12];
   static char buf2[12];
   int i,j;

   if (num==0)
   {
      buf[0] = '0';
      buf[1] = '\0';
      return &buf[0];
   }

   for(i=0;i<11 && num!=0;i++)
   {
      buf[i]=(char)((num%10)+'0');
      num/=10;
   }
   buf2[i] = '\0';
   j = 0;
   i--;
   for(;i>=0;i--){
         buf2[i]=buf[j];
         j++;
   }
   return &buf2[0];
}

int pow_base2(int pow){
	int res = 1;
	while(pow){
		res = res * 2;
		pow--;
	}

	return res;
}

int abs(int num){
	if(num<0) return -num;
	else return num;
}

/******************************** CS CONTROLLER FUNCTIONS *********************************************/

//Gets the CS path size
unsigned int hops_count;

//------ Variaveis globais -------
//(ROUTER_NUMBER * CS_NETS * PORT_NUMBER) bytes
char cs_inport[XCLUSTER][YCLUSTER][CS_NETS][PORT_NUMBER];

//Lista dos vizinhos, utilizanda dentro da funcao hadlock
// 2 * (ROUTER_NUMBER) bytes
unsigned short int nlist[ROUTER_NUMBER];
int last_nlist;

//Lista dos propagacao, utilizanda dentro da funcao hadlock e retrace
// 2 * (ROUTER_NUMBER) bytes
unsigned short int plist[ROUTER_NUMBER];
int last_plist;

char detour[XCLUSTER][YCLUSTER];

unsigned int subnet_selector = 0;


#if PATH_DEBUG
//DETELE THOSE VARIABLES, THEY ARE ONLY USED TO PRINT THE PATH
unsigned int print_paths[ROUTER_NUMBER];
unsigned int print_controller[NC_NUMBER];
#endif
int print_path_size = 0;
//DELETE VARIABLES ABOVE


/********** New variables *******************/
//Tabela dos outros cluster é recebida sob demanda no lock do CS
//Guarda o status das portas de entrada da borda do cluster, como são 4 bordas, possui tamanho 4. Cada elemento, hot encoded,
//guarda o respectivo status do roteador da esquerda para direita.
//unsigned int cluster_border_input[NC_NUMBER][5];
//cluster_border_input stores if a router has its input free and is propagable
//The function is_PE_propagable feeds this array
unsigned short int 	cluster_border_input[CLUSTER_X_NUMBER][CLUSTER_Y_NUMBER][CS_NETS][4];
unsigned short int 	cluster_border_output[CLUSTER_X_NUMBER][CLUSTER_Y_NUMBER][CS_NETS][4];
char				available_controllers[CLUSTER_X_NUMBER][CLUSTER_Y_NUMBER][CS_NETS];


unsigned int 		cluster_path[NC_NUMBER];
unsigned int 		global_path_size;
unsigned short int 	max_X_border_occuptation;
unsigned short int 	max_Y_border_occuptation;

//###### Statistics variables
//Local data
unsigned short int subnet_utilization[CS_NETS];
//Global Routing
unsigned short int global_subnet_utilization[CLUSTER_X_NUMBER][CLUSTER_Y_NUMBER][CS_NETS];


//########################################
//Hadlock function that enable it to find either at global or local level
unsigned int MAX_GRID_X = XCLUSTER; //default values
unsigned int MAX_GRID_Y = YCLUSTER; //default values
unsigned char is_global_search = 0;  //default values
//#######################################

//Variables that identifies the controller
int cluster_id; //Identifies the ID, ex: 0, 1, 2, 3
int x_cluster_addr, y_cluster_addr; //Identifies the X and Y addr;
int cluster_addr; //Identifies

void enable_global_routing(){
	MAX_GRID_X = CLUSTER_X_NUMBER;
	MAX_GRID_Y = CLUSTER_Y_NUMBER;
	is_global_search = 1;
}

void enable_local_routing(){
	MAX_GRID_X = XCLUSTER;
	MAX_GRID_Y = YCLUSTER;
	is_global_search = 0;
}


int is_element_reachable(int sx, int sy, int tx, int ty, int input, int mp){

	if (tx < 0 || tx > MAX_GRID_X-1 || ty < 0 || ty > MAX_GRID_Y-1)
		return 0;

	if (is_global_search){

		//printf("Global source %dx%d target %dx%d\n", sx, sy, tx, ty);

		//If the manager was failed in a previous connection attempt for the same path them denies it use
		if (!available_controllers[tx][ty][mp])
			return 0;

		switch (input) {
			case EAST:
				return (int) (cluster_border_output[sx][sy][mp][WEST] | cluster_border_input[tx][ty][mp][EAST]) < max_Y_border_occuptation;
			case WEST:
				return (int) (cluster_border_output[sx][sy][mp][EAST] | cluster_border_input[tx][ty][mp][WEST]) < max_Y_border_occuptation;
			case NORTH:
				return (int) (cluster_border_output[sx][sy][mp][SOUTH] | cluster_border_input[tx][ty][mp][NORTH]) < max_X_border_occuptation;
			case SOUTH:
				return (int) (cluster_border_output[sx][sy][mp][NORTH] | cluster_border_input[tx][ty][mp][SOUTH]) < max_X_border_occuptation;
		}

	}

	return (cs_inport[tx][ty][mp][input] == -1);
}

void clear_search(){

	last_nlist = 0;
	last_plist = 0;

	for(int i=0; i<XCLUSTER; i++){
		for(int j=0; j<YCLUSTER; j++){
			detour[i][j] = -1;
		}
	}
}

/**Inicializa o array cluster_border_input, colocando 0 em cada elemento, ou seja, todas as interfaces do roteador estao livres e o indice de saturacao eh 0
 */
void initit_cluster_borders(){
	
	for(int x=0; x<CLUSTER_X_NUMBER; x++){
		for(int y=0; y<CLUSTER_Y_NUMBER; y++){
			for(int s=0; s<CS_NETS; s++){
				for(int j=0; j<4; j++){
					cluster_border_input[x][y][s][j] = 0;
					cluster_border_output[x][y][s][j] = 0;
				}
			}
		}
	}

	max_X_border_occuptation = (unsigned short int) (pow_base2(XCLUSTER) - 1);
	max_Y_border_occuptation = (unsigned short int) (pow_base2(YCLUSTER) - 1);

}

//Inicializa a estrutura router. Essa funcao eh chamada somente no boot do kernel
void init_search_path(){

	Puts("\nInitializing search path\n");

	for(int x=0; x<XCLUSTER; x++)
		for (int y=0; y<YCLUSTER; y++ )
			for(int k=0; k<PORT_NUMBER; k++)
				for(int p=0; p<CS_NETS; p++)
					cs_inport[x][y][p][k] = -1;

	clear_search();

	//Clear statistics
	for(int p=0; p<CS_NETS; p++)
		subnet_utilization[p] = 0;

}


int Manhattan(int xi, int yi, int xj, int yj){
	return (abs(xi - xj) + abs(yi - yj));
}

//Remove o elemento de nlist com menor detour, remove de tras para frente, ordena a lista
int remove_min_detour_nlist(){

	int x_add, y_add, min_i;
	int addr_ret;
	char min_detour;

	x_add = nlist[last_nlist-1] >> 8;
	y_add = nlist[last_nlist-1] & 0xFF;
	min_detour = detour[x_add][y_add];
	min_i = last_nlist-1;

	//Remove the first in element with lowest detour
	for(int i=last_nlist-2; i >= 0; i--){
		x_add = nlist[i] >> 8;
		y_add = nlist[i] & 0xFF;
		if (detour[x_add][y_add] < min_detour){
			min_detour = detour[x_add][y_add];
			min_i = i;
		}
	}

	addr_ret = nlist[min_i];

	//Sorts the FIFO
	for(int i = min_i; i<last_nlist-1; i++)
		nlist[i] = nlist[i+1];

	last_nlist--;

	return addr_ret;
}

#if SDN_DEBUG
void print_port(int p){
	switch (p) {
		case LOCAL_IN:
			Puts("LOCAL_IN");
			break;
		case LOCAL_OUT:
			Puts("LOCAL_OUT");
			break;
		case NORTH:
			Puts("NORTH");
			break;
		case SOUTH:
			Puts("SOUTH");
			break;
		case WEST:
			Puts("WEST");
			break;
		case EAST:
			Puts("EAST");
			break;
		default:
			Puts("UNK");
			break;
	}
}

void print_router_status(int subnet){
	Puts("\n\n\t\tE\tW\tN\tS\tLOUT\tLIN\n");
	for(int x=0; x<XCLUSTER; x++){

		for(int y=0; y<YCLUSTER; y++){

			Puts("Router "); Puts(itoa(x)); Puts("x"); Puts(itoa(y)); Puts(" : ");

			for(int k=0; k<PORT_NUMBER; k++){ //4 is the number of border in each cluster

				Puts("\t"); Puts(itoa(cs_inport[x][y][subnet][k]));
			}
			Puts("\n");
		}
	}
}
#endif

/*
 Retrace: o objetivo e percorrer a lista de propagacao plist, de tras pra frente (target para source), e ir alocando as portas dos roteadores
 correspondentes
 * mp : define the subnet number
 * beg_inport: defines the input port at the beggining of the path
 * end_outport: defines the outport at the end of the path
 */
void retrace(int mp, int beg_inport, int end_outport){

	int x, y, Nx, Ny, inport, outport_next=0, outport, cfg_router;

	if(is_global_search)//Codigo adicionado somente no modo busca global
		global_path_size = 0;//cluster_path_size is a global variable used to controll the lengh of cluster path
	//This variable receives zero here because a new cluster path will be setup

	print_path_size = 0;//**** ONly debugging delete after

	hops_count = 0;
	//Armazena em x e y os ultimos elementos da lista plist
	x = plist[last_plist-1] >> 8;
	y = plist[last_plist-1] & 0xFF;

	//Aloca a porta do roteador de destino caso o modo do hadlock eh local
	if (!is_global_search && end_outport == LOCAL_OUT)
		cs_inport[x][y][mp][LOCAL_OUT] = 9;//9 representa somente o sinal de ocupado, pois a porta local nao tem pra onde sair

	//New conditiona f (last_plist == 1){ added due global routing effects
	//Esse codigo eh um caso especial, onde o origem e destino sao iguais
	//Isso pode acontecer pois no modo global routing pode ficar um roteador sozinho em um
	//cluster. O que eh feito nesse if eh um cuidado pra alocar somente as portas que realmente importam. Sempre a porta de entrada eh alocada, 
	//ja a saida, esta eh verificado se ela nao for igual a LOCAL_OUT entao o roteador vizinho eh que vai armazenar o status da saida
	if (last_plist == 1){
		
#if SDN_DEBUG
		if (is_global_search){
			Puts("\nRouter "); Puts(itoa(x)); Puts("x"); Puts(itoa(y));
		} else {
			Puts("\nRouter "); Puts(itoa(x+x_offset)); Puts("x"); Puts(itoa(y+y_offset));
		}
		Puts(" - "); print_port(beg_inport); Puts(" -> "); print_port(end_outport); Puts("\n");
#endif
		if (!is_global_search){//Codigo adicionado somente no modo busca global
			cs_inport[x][y][mp][beg_inport] = end_outport;
			/************** router configuration *************************/
			cfg_router = ((x+x_offset) << 8) | (y+y_offset);
			ConfigRouter(cfg_router, (beg_inport << 8) | end_outport, mp);
			/**************************************************************/

			if(beg_inport != LOCAL_IN && end_outport != LOCAL_OUT)
				hops_count++;

#if PATH_DEBUG
			print_paths[print_path_size] = ((x+x_offset) << 8) | (y+y_offset);//Apagar ***
#endif
			print_path_size++;

			subnet_utilization[mp] += hops_count;
#if SDN_DEBUG
			Puts("Retrace2 updated subnet utilization to: "); Puts(itoa(subnet_utilization[mp])); Puts("\n");
#endif
		}
		
		return;
	}


	//outport = LOCAL_OUT;
	outport = end_outport;
	//Aponta i para o penultimo elemento

	for (int i=last_plist-1; i>=0; i--){

		for(int k=0; k<i; k++){
			Nx = plist[k] >> 8;
			Ny = plist[k] & 0xFF;

			if (Manhattan(x, y, Nx, Ny) == 1){

				//Check if can go directly
				if (Nx > x)
					inport = EAST;
				else if (Nx < x)
					inport = WEST;
				else if (Ny > y)
					inport = NORTH;
				else
					inport = SOUTH;

				if (is_element_reachable(Nx, Ny, x, y, inport, mp)){
				//if (cs_inport[x][y][mp][inport] == -1){
					i=k;
					break;
				}
			}
		}

		Nx = plist[i] >> 8;
		Ny = plist[i] & 0xFF;

		//Escolhe a porta que sera alocada no roteador
		if (Nx > x) { //Nx++
			inport = EAST;
			outport_next = WEST;
		} else if (Nx < x) {//Nx--
			inport = WEST;
			outport_next = EAST;
		} else if (Ny > y) { //Ny++
			inport = NORTH;
			outport_next = SOUTH;
		} else {			//Ny--
			inport = SOUTH;
			outport_next = NORTH;
		}
		//Configura o elemento atual alocando a porta selecionada
		if (!is_global_search){
			cs_inport[x][y][mp][inport] = outport;
			if (outport != LOCAL_OUT)
				hops_count++;
			/************** router configuration *************************/
			cfg_router = ((x+x_offset) << 8) | (y+y_offset);
			ConfigRouter(cfg_router, (inport << 8) | outport, mp);
			/**************************************************************/

#if PATH_DEBUG
			print_paths[print_path_size] = ((x+x_offset) << 8) | (y+y_offset);//**Apagar
#endif
			print_path_size++;

		} else //Codigo adicionado somente no modo busca global
			//								    |-cluster addr(16b)--|--inport (8b)-|--outport(8b)-|
			cluster_path[global_path_size++] = (x << 24) | (y << 16) | (inport << 8) | outport;
#if SDN_DEBUG
		if (is_global_search){
			Puts("\nRouter "); Puts(itoa(x)); Puts("x"); Puts(itoa(y));
		} else {
			Puts("\nRouter "); Puts(itoa(x+x_offset)); Puts("x"); Puts(itoa(y+y_offset));
		}
		Puts(" - "); print_port(inport); Puts(" -> "); print_port(outport);
#endif

		outport = outport_next;

		//Incrementa pro proximo elemento
		x = Nx;
		y = Ny;

	}

#if SDN_DEBUG
	if (is_global_search){
		Puts("\nRouter "); Puts(itoa(x)); Puts("x"); Puts(itoa(y));
	} else {
		Puts("\nRouter "); Puts(itoa(x+x_offset)); Puts("x"); Puts(itoa(y+y_offset));
	}
	Puts(" - "); print_port(beg_inport); Puts(" -> "); print_port(outport); Puts("\n");
#endif

	//Aloca a porta do roteador origem caso o modo for local
	if (!is_global_search){
		cs_inport[x][y][mp][beg_inport] = outport;
		if(beg_inport != LOCAL_IN && outport != LOCAL_OUT)
			hops_count++;
		subnet_utilization[mp] += hops_count;

		/************** router configuration *************************/
		cfg_router = ((x+x_offset) << 8) | (y+y_offset);
		ConfigRouter(cfg_router, (beg_inport << 8) | outport, mp);
		/**************************************************************/

#if PATH_DEBUG
		print_paths[print_path_size] = ((x+x_offset) << 8) | (y+y_offset);//Apagar ***
#endif
		print_path_size++;
#if SDN_DEBUG
		Puts("Retrace updated subnet utilization to: "); Puts(itoa(subnet_utilization[mp])); Puts("\n");
#endif
	} else {//Codigo adicionado somente no modo busca global
		//								    |-cluster addr(16b)--|--inport (8b)-|--outport(8b)-|
		//cluster_path[global_path_size++] = (x << 24) | (y << 16) | (LOCAL_IN << 8) | outport;
		cluster_path[global_path_size++] = (x << 24) | (y << 16) | (beg_inport << 8) | outport;
	}

}


/*
 Funcao hadlock: procura o caminho minimo existe entre dois roteadores. O algoritmo tem como base selecionar um roteador em p_vertex.
 Na sequencia todos os vizinhos sao preenchidos com um numero de detour. O proximo p_vertex será o vizinho com o menor numero detour.
 */
int hadlock(int Vi_x, int Vi_y, int target_x, int target_y, int mp, int min_path){

	int path_found, Nx, Ny, input_port=-1, p_vertex;

	path_found = 0;

	clear_search();

	detour[Vi_x][Vi_y] = 0;

	//Inicializa p_vertex com o roteador origem
	p_vertex = Vi_x << 8 | Vi_y;

	//Adiciona o roteador origem como primeiro elemento na lista de propagacao
	plist[last_plist] = p_vertex;
	last_plist++;
	
	//Special code added to support global routing mode, where sometimes this if can be true
	////New conditiona if (last_plist == 1){ added due global routing effects
	if (Vi_x == target_x && Vi_y == target_y)
		return 1;

	// -------------------------------------------- FIM INICIALIZACAO -----------------------------------------------------------------------

	//Enquanto houver um roteador valido de propagacao e o caminho ainda nao foi encontrado. Pior caso: dim.X x dim.Y
	while( p_vertex != -1 && path_found == 0){

		//Atualiza Vi router com o proximo routeador de propagacao
		Vi_x = p_vertex >> 8;
		Vi_y = p_vertex & 0xFF;

		//Min path controll
		if (min_path && detour[Vi_x][Vi_y] > 0) {
			//Puts("\n############ AQUI\n");
			return 0;
		}

		//Configura p_vertex para nulo visando saior do while caso nenhum vizinho seja encontrado
		p_vertex = -1;

		//Percorre os 4 vizinhos de cada roteador
		for(int n = 0; n < 4; n++){

			//Nx e Ny sao possuem as coordenadas x e y do proximo roteador que podera ser o p_vertex
			Nx = Vi_x;
			Ny = Vi_y;

			switch (n) {
				case EAST: 	Nx++; input_port = WEST;  break;
				case WEST: 	Nx--; input_port = EAST;  break;
				case NORTH: Ny++; input_port = SOUTH; break;
				case SOUTH: Ny--; input_port = NORTH; break;
			}

			if (detour[Nx][Ny] != -1 || (!is_element_reachable(Vi_x, Vi_y, Nx, Ny, input_port, mp)))
				continue;

			//Calcula o numero detour para o vizinho
			if (Manhattan(Nx, Ny, target_x, target_y) >= Manhattan(Vi_x, Vi_y, target_x, target_y))
				detour[Nx][Ny] = detour[Vi_x][Vi_y] + 1;
			else
				detour[Nx][Ny] = detour[Vi_x][Vi_y];

			//Adiciona na lista de vizinho
			nlist[last_nlist] = (Nx << 8 | Ny);
			last_nlist++;

			//Se alcançou o destino encerra a procura
			if (Nx == target_x && Ny == target_y){
				path_found = 1;
				break;
			}
		} // fim: for(int n = 0; n < 4; n++)

		//Caminho nao existe pois nao achou nenhum possivel vizinho para ser o proximo roteador de propagacao
		if (last_nlist == 0) break;

		//Remove o ultimo vizinho adicionado com o menor detour
		p_vertex = remove_min_detour_nlist();

		//Adiciona na lista de propagacao
		plist[last_plist] = p_vertex;
		last_plist++;

	} // fim: while( p_vertex != -1 && path_found == 0)
	//Path found pode ser 0 ou 1. Se for 1 o caminho foi encotrado

	return path_found;
}

/*void clear_path(int source_x, int source_y, int target_x, int target_y, int mp, int path_id){

	int x, y, Nx, Ny, input_port, outport, outport_next;



}*/

unsigned int CS_connection_setup(int source_x, int source_y, int target_x, int target_y){

	int min_path = 1;//true
	int path_found = 0;
	int i = 0;

#if SDN_DEBUG
	Puts("\n------CS connection setup---------\n");
#endif
	while(i < CS_NETS){

		if (subnet_selector == CS_NETS-1)
			subnet_selector = 0;
		else
			subnet_selector++;

#if SDN_DEBUG
		Puts("\nTrying CS for subnet = "); Puts(itoa(subnet_selector)); Puts("\n");
#endif

		//Only tries to establish connection if the input port of source and output of target are available
		if ( cs_inport[source_x][source_y][subnet_selector][LOCAL_IN] == -1 && cs_inport[target_x][target_y][subnet_selector][LOCAL_OUT] == -1 ){
#if SDN_DEBUG
			Puts("Local path oK\n");
#endif
			if ( hadlock(source_x, source_y, target_x, target_y, subnet_selector, min_path) ){
				path_found = 1;
				
				retrace(subnet_selector, LOCAL_IN, LOCAL_OUT);
				
				break;
			}
		}
		i++;
		//If the last subnet not lead to the minimal path, them, set min_path = 1 and retry
		if ( min_path && i == CS_NETS){
			min_path = 0;//false
			i = 0;
#if SDN_DEBUG
		Puts("min_path changed to 1\n");
#endif
		}

	}

	if (path_found)
		//Hot encoded return - leftmost 16 bits store the subnet, rightmost 16 bits store the path_id
		return subnet_selector;

#if SDN_DEBUG
	Puts("no path found\n");
#endif
	return -1;
}

int CS_connection_release(int source_x, int source_y, int target_x, int target_y, int mp, int inport){

	int Nx, Ny, x, y;
	char outport, next_inport = 0;

#if SDN_DEBUG
	Puts("\n------CS connection release---------\n");
#endif

	hops_count = 0;

	x = source_x;
	y = source_y;

	//Pega a porta de saida do primeiro roteador

	Nx = x;
	Ny = y;

	while(x != target_x || y != target_y){

		outport = cs_inport[x][y][mp][inport];

#if SDN_DEBUG
		Puts("\nRouter "); Puts(itoa(x+x_offset)); Puts("x"); Puts(itoa(y+y_offset));
		Puts(" - "); print_port(inport); Puts(" -> "); print_port(outport);
#endif
		if(inport != LOCAL_IN && outport != LOCAL_OUT)
			hops_count++;

		cs_inport[x][y][mp][inport] = -1;


		switch (outport) {
			case EAST:
				next_inport = WEST;
				Nx++;
				break;
			case WEST:
				next_inport = EAST;
				Nx--;
				break;
			case NORTH:
				next_inport = SOUTH;
				Ny++;
				break;
			case SOUTH:
				next_inport = NORTH;
				Ny--;
				break;
		}

		if (Nx < 0 || Nx > XCLUSTER-1 || Ny < 0 || Ny > YCLUSTER-1){
			subnet_utilization[mp] -= hops_count;
#if SDN_DEBUG
			Puts("\nRelease2 updated subnet utilization to: "); Puts(itoa(subnet_utilization[mp])); Puts("\n");
#endif
			return (x << 8 | y) << 16 | next_inport;
		}

		x = Nx;
		y = Ny;
		inport = next_inport;
	}


#if SDN_DEBUG
		Puts("\nRouter "); Puts(itoa(x+x_offset)); Puts("x"); Puts(itoa(y+y_offset));
		Puts(" - "); print_port(inport); Puts(" -> "); print_port(LOCAL_OUT);
#endif
	//Clear the target router
	cs_inport[x][y][mp][inport] = -1;
	cs_inport[x][y][mp][LOCAL_OUT] = -1;

	subnet_utilization[mp] -= hops_count;
#if SDN_DEBUG
	Puts("\nRelease updated subnet utilization to: "); Puts(itoa(subnet_utilization[mp])); Puts("\n");
#endif

	return -1;
}


#endif /* NOC_MANAGER_H_ */
