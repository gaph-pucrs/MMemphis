/*
 * qos_learning.c
 *
 *  Created on: May 3, 2017
 *      Author: ruaro
 */

#include "qos_learning.h"
#include "utils.h"

#define MAX_STATES	4
#define MAX_ACTIONS	4

//Possible states->actions transitions
int P[MAX_STATES][MAX_ACTIONS] = {
		{1, 0, 0, 0},
		{1, 1, 0, 0},
		{1, 1, 1, 0},
		{1, 1, 1, 1}
};

int R[MAX_STATES][MAX_ACTIONS] = {
		{1000, -10, -250, -500},
		{1000, -10, -250, -500},
		{1000, -10, -250, -500},
		{1000, -10, -250, -500}
};

int Q[MAX_STATES][MAX_ACTIONS];

int discount = 8;//0.8f;
int learning_rate = 5; //0.5f;

//OBS flat funciona mas sempre se deve colocar o f
void init_learning(){

	//Initializes matrix Q as a zero
	for(int s=0; s<MAX_STATES; s++){
		for(int a=0; a<MAX_ACTIONS; a++){
			Q[s][a] = 0;
		}
	}

}

/*
char * get_action(int action){
	switch (action) {
		case Nothing:
			return("Nothing");
		case Deep_Analysis:
			return("Deep Analyses");
		case MigrateBE:
			return("Migrates BE");
		case MigrateRT:
			return("Migrates RT");
		default:
			break;
	}
	return("ERR: no action found!");
}
*/
char * get_state(int state){
	switch (state) {
		case NORMAL:
			return("NORMAL");
		case ATTENTION:
			return("ATTENTION");
		case CRITICAL:
			return("CRITICAL");
		default:
			break;
	}
	return("ERR: no state found!");
}


int max(int * q_array){

	int higher_Q = -100;

	for(int i=0; i<MAX_ACTIONS; i++){
		if (higher_Q < q_array[i])
			higher_Q = q_array[i];
	}

	return higher_Q;
}


void print_matrix(int m[MAX_STATES][MAX_ACTIONS]){

	for(int s=0; s<MAX_STATES; s++) {
		puts("\n");
		for(int a=0; a<MAX_ACTIONS; a++){
			puts(itoa(m[s][a])); puts("\t");
		}
	}
	puts("\n");
}

/**
 * \param ls Last State
 * \param la Last Action
 * \param ns Next State - this state is the result of the last action
 */

int learning(int ls, int la, int ns){

	int na; //next action
	int max_ns_Q;

	/*puts("-------------- learning -----------------------\n");
	puts("Last action was = "); puts(get_action(la)); puts("\n");
	puts("Last state was = "); puts(get_state(ls)); puts("\n");*/
	//puts(".States: from "); puts(get_state(ls)); puts(" to "); puts(get_state(ns));

	//--------------------- learning --------------------------------
	//Gets the maximum Q value from the next state
	max_ns_Q = max(Q[ns]);

	//Updates the Q matrix based on the reward of the last action
	//The divisons by 10 are only because I am working with integer instead float
	Q[ls][la] = Q[ls][la] + (learning_rate * ( R[ls][ns] + (discount * max_ns_Q / 10) - Q[ls][la]) ) / 10;

	//print_matrix(Q);

	//--------------------- deciding ----------------------------------
	//Select one among all possible actions for the current action from the current state.
	na = 0;
	for(int a=0; a<MAX_ACTIONS; a++){
		if(P[ns][a] && Q[ns][na] < Q[ns][a])
			na = a;
	}

	//puts(get_state(ns)); puts("\t"); puts(get_action(na)); puts("\n");

	return na;

}



