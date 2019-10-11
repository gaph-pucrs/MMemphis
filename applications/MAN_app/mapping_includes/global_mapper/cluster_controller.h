/*
 * global_mapper.h
 *
 *  Created on: Jul 3, 2019
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_
#define APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_

#include "../../common_include.h"

typedef struct {
	int x_pos;					//!< Stores the application id
	int y_pos;					//!< Stores the application status
	int free_resources;
} Cluster;


#define MAPPING_CLUSTER_NUMBER	(MAPPING_X_CLUSTER_NUM*MAPPING_Y_CLUSTER_NUM)
#define SDN_CLUSTER_NUMBER		(MAPPING_X_CLUSTER_NUM*MAPPING_Y_CLUSTER_NUM)

/*Global Variables*/
Cluster clusters[MAPPING_CLUSTER_NUMBER];
unsigned int total_mpsoc_resources = (MAX_LOCAL_TASKS * XDIMENSION * YDIMENSION);
unsigned int pending_app_req = 0;


void intilize_clusters(){
	unsigned int cluster_id = 0;
	Cluster * cl_ptr;

	for(int y=0; y<MAPPING_X_CLUSTER_NUM; y++){
		for(int x=0; x<MAPPING_Y_CLUSTER_NUM; x++){
			cl_ptr = &clusters[cluster_id];
			//XY holds the address of the PE at most at bottom-left of each cluster
			cl_ptr->x_pos = x;
			cl_ptr->y_pos = y;
			cl_ptr->free_resources = (MAX_LOCAL_TASKS * MAPPING_XCLUSTER * MAPPING_YCLUSTER);
			cluster_id++;
		}
	}
	Puts("Clusters initialized\n");
}


/**Selects a cluster to insert an application
 * \param app_task_number Number of task of requered application
 * \return > 0 if mapping OK, -1 if there is not resources available
*/
int search_cluster(int app_task_number) {

	int selected_cluster = -1;
	int freest_cluster = 0;

	for (int i=0; i<MAX_MAPPING_TASKS; i++){

		if (clusters[i].free_resources > freest_cluster){
			selected_cluster = i;
			freest_cluster = clusters[i].free_resources;
		}
	}

	return selected_cluster;
}

int get_cluster_index_from_PE(unsigned int pe_addr){

	int x,y;

	x = pe_addr >> 8;
	y = pe_addr & 0xFF;

	y = (int) y / MAPPING_Y_CLUSTER_NUM;
	x = (int) y / MAPPING_X_CLUSTER_NUM;

	return ((y*MAPPING_X_CLUSTER_NUM)+x);
}


/** Allocate resources to a Cluster by decrementing the number of free resources. If the number of resources
 * is higher than free_resources, then free_resourcers receives zero, and the remaining of resources are allocated
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to allocated. Normally is the number of task of an application
 */
void allocate_cluster_resource(int cluster_index, int nro_resources){

	//Puts(" Cluster address "); Puts(itoh(clusters[cluster_index].x_pos << 8 | clusters[cluster_index].y_pos)); Puts(" resources "); Puts(itoa(clusters[cluster_index].free_resources));

	//if (clusters[cluster_index].free_resources >= nro_resources){
		clusters[cluster_index].free_resources -= nro_resources;
	/*} else {
		Puts("ERROR: cluster has not enought resources\n"); putsv(" index ", cluster_index);
		while(1);
	}*/

	//putsv(" ALLOCATE - nro resources : ", clusters[cluster_index].free_resources);
}

/** Release resources of a Cluster by incrementing the number of free resources according to the nro of resources
 * by reclustering
 * \param cluster_index Index of cluster to allocate the resources
 * \param nro_resources Number of resource to release. Normally is the number of task of an application
 */
void release_cluster_resources(int cluster_index, int nro_resources){

	//Puts(" Cluster address "); Puts(itoh(clusters[cluster_index].x_pos << 8 | clusters[cluster_index].y_pos)); Puts(" resources "); Puts(itoa(clusters[cluster_index].free_resources));

	clusters[cluster_index].free_resources += nro_resources;

    //putsv(" RELEASE - nro resources : ", clusters[cluster_index].free_resources);
}




#endif /* APPLICATIONS_MAN_CONTROL_APPLICATION_CLUSTER_CONTROLLER_H_ */
