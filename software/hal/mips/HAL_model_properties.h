/*
 * HAL_model.h
 *
 *  Created on: Sep 16, 2019
 *      Author: ruaro
 */

#ifndef _HAL_MODEL_PROPERTIES_
#define _HAL_MODEL_PROPERTIES_

#define MAX_LOCAL_TASKS             1  //max task allowed to execute into a single processor
#define PAGE_SIZE                   32768//bytes
#define MAX_TASKS_APP               20 //max number of tasks for the APPs described into testcase file
#define SUBNETS_NUMBER              2 //number of subnets implemented by NoC
#define XDIMENSION                  2     //mpsoc  x dimension
#define YDIMENSION                  2     //mpsoc  y dimension
#define XCLUSTER                    2     //cluster x dimension
#define YCLUSTER                    2     //cluster y dimension
#define CLUSTER_NUMBER              1     //total number of cluster
//Peripherals
#define PERIPHERALS_NUMBER          1      //max number of peripherals
#define APP_INJECTOR            0xc0000101    //This number is hot encoded (1 k bit + 3 n bits + 12 zeros + 8 x bits + 8 y bits). k bit when on enable routing to external peripheral, n bits signals the port between peripheral and PE, x bits stores the X address of PE, y bits stores the Y address of PE

#endif
