/*
 * prod.c
 *
 *  Created on: 07/03/2013
 *      Author: mruaro
 */

#include <api.h>
#include <stdlib.h>
#include "prod_cons_std.h"

Message msg;

int main()
{

	int i;
	volatile int t;


	Echo("Inicio da aplicacao prod");

	for(i=0;i<MSG_SIZE;i++) msg.msg[i]=i;
	msg.length = MSG_SIZE;


	for(i=0; i<PROD_CONS_ITERATIONS; i++){
		Send(&msg, cons);
		//for(t=0;t<550; t++); //50%
		//for(t=0;t<3860; t++); //10%
	}


	Echo("Fim da aplicacao prod");
	exit();

}


