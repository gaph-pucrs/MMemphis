#include <api.h>
#include <stdlib.h>

#include "syn_std.h"

//Message structure of MEMPHIS, provided by api.h
Message msg;

void main()
{
    Echo("Task D started at time ");
	Echo(itoa(GetTick()));

	for(int i=0;i<SYNTHETIC_ITERATIONS;i++)
	{
		Echo("New round");
		Receive(&msg, taskB);
		Echo("B");
		Echo(itoa(msg.msg[0]));
		Receive(&msg, taskC);
		Echo("C");
		Echo(itoa(msg.msg[0]));

	}
/*
	Echo("Final message");
	for(int j=0; j<msg.length; j++){
		Echo(itoa(msg.msg[j]));
	}*/


    Echo("Task D finished at time");
    Echo(itoa(GetTick()));
	exit();
}
