#include <api.h>
#include <stdlib.h>

#define NUM_NODES 					16		//16 for small input;
#define NPROC 						5
#define INFINITY					9999
#define KILL						99999

int main()
{
	Message msg;
	int i, j, v;
	int source = 0;
	int q[NUM_NODES];
	int dist[NUM_NODES];
	int prev[NUM_NODES];
	int shortest, u;
	int alt;
	int calc = 0;

	int AdjMatrix[NUM_NODES][NUM_NODES];


	while(1){
		msg.length = NUM_NODES;
		for (i=0; i<NUM_NODES; i++) {
			Receive(&msg, divider);
			for (j=0; j<NUM_NODES; j++)
				AdjMatrix[i][j] = msg.msg[j];
		}
		calc = AdjMatrix[0][0];
		if (calc == KILL) break;

		for (i=0;i<NUM_NODES;i++){
			dist[i] = INFINITY;
			prev[i] = INFINITY;
			q[i] = i;
		}
		dist[source] = 0;
		//u = 0;

		for (i=0;i<NUM_NODES;i++) {
			shortest = INFINITY;
			for (j=0;j<NUM_NODES;j++){
				if (dist[j] < shortest && q[j] != INFINITY){		
					shortest = dist[j];
					u = j;
				}
			}
			q[u] = INFINITY;

			for (v=0; v<NUM_NODES; v++){
				if (q[v] != INFINITY && AdjMatrix[u][v] != INFINITY){
					alt = dist[u] + AdjMatrix[u][v];
					if (alt < dist[v]){
						dist[v] = alt;
						prev[v] = u;
					}
				}
			}
		}
	}

	msg.length = NUM_NODES*2;
	for (i=0;i<NUM_NODES;i++)
		msg.msg[i] = dist[i];

	for (i=0;i<NUM_NODES;i++)
		msg.msg[i+NUM_NODES] = prev[i];

	// Send(&msg, print_dij);

	Echo(itoa(GetTick()));
	Echo("Dijkstra_2 finished.");

	exit();
}

