#include <api.h>
#include <stdlib.h>

#include "syn_std.h"

//MEMPHIS message structure
Message msg1;
Message msg2;

void main()
{
    Echo("Task A started at time ");
	Echo(itoa(GetTick()));

	for(int i=0;i<SYNTHETIC_ITERATIONS;i++)
	{
		//Compute and send something
		msg1.msg[0] = i;
		msg1.length = 1;

		//compute(&msg1.msg);
		Send(&msg1,taskB);

		//Compute and send something
		msg2.msg[0] = SYNTHETIC_ITERATIONS-i;
		msg2.length = 1;

		//compute(&msg2.msg);
		Send(&msg2,taskC);
	}

    Echo("Task A finished at time");
    Echo(itoa(GetTick()));
	exit();
}
