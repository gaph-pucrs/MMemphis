/*
 * qos_learning.h
 *
 *  Created on: May 3, 2017
 *      Author: ruaro
 */

#ifndef QOS_LEARNING_H_
#define QOS_LEARNING_H_

enum states {NORMAL, ATTENTION, CRITICAL};

void init_learning();

int learning(int, int, int);

char * get_state(int);

#endif /* QOS_LEARNING_H_ */
