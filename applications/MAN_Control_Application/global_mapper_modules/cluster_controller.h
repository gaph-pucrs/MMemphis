/*
 * global_mapper.h
 *
 *  Created on: Jul 3, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_

#include "../common_include.h"

typedef struct {
	int x_pos;					//!< Stores the application id
	int y_pos;					//!< Stores the application status
	unsigned int free_resources;
} Cluster;


/*Global Variables*/
Cluster clusters[CLUSTER_NUMBER];
unsigned int total_mpsoc_resources = (MAX_LOCAL_TASKS * XDIMENSION * YDIMENSION);
unsigned int pending_app_req = 0;

/**Selects a cluster to insert an application
 * \param app_task_number Number of task of requered application
 * \return > 0 if mapping OK, -1 if there is not resources available
*/
int search_cluster(int app_task_number) {

	int selected_cluster = -1;
	int freest_cluster = 0;

	for (int i=0; i<CLUSTER_NUMBER; i++){

		if (clusters[i].free_resources > freest_cluster){
			selected_cluster = i;
			freest_cluster = clusters[i].free_resources;
		}
	}

	return selected_cluster;
}


/*Total MPSoC resources controll*/
int decrease_avail_mpsoc_resources(unsigned int num_tasks){
	if (num_tasks > total_mpsoc_resources)
		return 0;

	total_mpsoc_resources -= num_tasks;
	return 1;
}

void increase_avail_mpsoc_resources(int num_tasks){
	total_mpsoc_resources += num_tasks;

	while (total_mpsoc_resources > (MAX_LOCAL_TASKS * XDIMENSION * YDIMENSION))
		Puts("ERROR: total_mpsoc_resources higher than MAX\n");
}


/** Allocate resources to a Cluster by decrementing the number of free resources. If the number of resources
 * is higher than free_resources, then free_resourcers receives zero, and the remaining of resources are allocated
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to allocated. Normally is the number of task of an application
 */
void allocate_cluster_resource(int cluster_index, int nro_resources){

	Puts("\n\n Cluster address "); Puts(itoh(clusters[cluster_index].x_pos << 8 | clusters[cluster_index].y_pos)); Puts(" resources "); Puts(itoa(clusters[cluster_index].free_resources));

	if (clusters[cluster_index].free_resources > nro_resources){
		clusters[cluster_index].free_resources -= nro_resources;
	} else {
		clusters[cluster_index].free_resources = 0;
	}

	Puts(" ALLOCATE - nro resources : ") Puts(itoa(clusters[cluster_index].free_resources)); Puts("\n\n");
}

/** Release resources of a Cluster by incrementing the number of free resources according to the nro of resources
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to release. Normally is the number of task of an application
 */
void release_cluster_resources(int cluster_index, int nro_resources){

	//puts(" Cluster address "); puts(itoh(cluster_info[cluster_index].master_x << 8 | cluster_info[cluster_index].master_y)); puts(" resources "); puts(itoa(cluster_info[cluster_index].free_resources));

	clusters[cluster_index].free_resources += nro_resources;

   // putsv(" RELEASE - nro resources : ", cluster_info[cluster_index].free_resources);
}




#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_ */
