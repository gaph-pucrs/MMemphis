/*
 * print_path.h
 *
 *  Created on: 17 de dez de 2018
 *      Author: mruaro
 */

#ifndef PRINT_PATH_H_
#define PRINT_PATH_H_

#include "noc_manager.h"

int printPathSize = 0;

#if PATH_DEBUG
#include "connection_request.h"

typedef struct {
	int cluster_addr;
	unsigned int path[XCLUSTER*YCLUSTER];
	int size;
}PrintPath;

PrintPath printPath[NC_NUMBER];


void clear_print(){
	printPathSize = 0;

	for(int i=0; i<NC_NUMBER; i++){
		printPath[i].size = 0;
		printPath[i].cluster_addr = -1;
	}
}


void addPath(int cluster_add, unsigned int * paths, int size){
	PrintPath * printPath_ptr = &printPath[printPathSize];

	printPath_ptr->size = size;
	printPath_ptr->cluster_addr = cluster_add;
	//Puts("Add path -- Cluster ID: "); Puts(itoa(cluster_add)); Puts(" size "); Puts(itoa(size)); Puts("\n");
	for(int i=0; i<size; i++){
		printPath_ptr->path[i] = paths[size-1-i];
		//Puts(itoa(printPath_ptr->path[i] >> 8)); Puts("x"); Puts(itoa(printPath_ptr->path[i] & 0xFF)); Puts("|");

	}
	printPathSize++;
	//Puts("\n Path size: "); Puts(itoa(printPathSize)); Puts("\n");
}


int write_path(int nc_size, unsigned int * msg){
	int curr_x, curr_y, full_addr;
	PrintPath * ptr = 0;
	int msg_index = 1;

	if (printPathSize == 1)
		nc_size = 1;


	for(int i=nc_size-1; i>=0; i--){
		curr_x = (cluster_path[i] & 0xFF000000) >> 24;//Extracts the cluster X address
		curr_y = (cluster_path[i] & 0x00FF0000) >> 16;//Extracts the cluster Y address

		if (printPathSize == 1){
			curr_x = x_cluster_addr;
			curr_y = y_cluster_addr;
		}

		//Puts("Selected cluster: "); Puts(itoa(curr_x)); Puts("x"); Puts(itoa(curr_y)); Puts("\n");


		full_addr = curr_x << 8 | curr_y;

		for(int j=0; j<NC_NUMBER; j++){
			if (printPath[j].cluster_addr == full_addr){
				ptr = &printPath[j];
				break;
			}
		}

		for(int k=0; k<ptr->size; k++){
			msg[msg_index++] = ptr->path[k];
			//Puts(itoa(ptr->path[k] >> 8)); Puts("x"); Puts(itoa(ptr->path[k] & 0xFF)); Puts("\n");
		}

		if (printPathSize == 1)
			break;

	}
	msg[0] = msg_index - 1;

	clear_print();

	return msg_index;
}

#endif


#endif /* PRINT_PATH_H_ */
