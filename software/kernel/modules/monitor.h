/*
 * low_level_monitor.h
 *
 *  Created on: Sep 19, 2019
 *      Author: ruaro
 */

#ifndef SOFTWARE_KERNEL_MODULES_MONITOR_H_
#define SOFTWARE_KERNEL_MODULES_MONITOR_H_

#include "TCB.h"

#define	ENABLE_APL_SLAVE 	0



void send_latency_miss(TCB *, int, int);

void init_profiling_window();

extern inline void check_profiling_need();

void send_slack_time_report();

extern inline void reset_last_idle_time();

extern inline void compute_idle_time();

#endif /* SOFTWARE_KERNEL_MODULES_MONITOR_H_ */
