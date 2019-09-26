/*
 * management_api.h
 *
 *  Created on: 28/09/2016
 *      Author: mruaro
 */

#ifndef _SERVICE_API_H_
#define _SERVICE_API_H_

//These macros must be a continuation of macros present into api.h
#define	REQSERVICEMODE	9
#define WRITESERVICE   	10
#define READSERVICE     11
#define	PUTS			12
#define CFGROUTER		13
#define NOCSENDFREE		14
#define INCOMINGPACKET	15
#define	GETNETADDRESS	16
#define	ADDTASKLOCATION	17
#define REMOVETASKLOCATION 18
#define	GETTASKLOCATION	19
#define SETMYID		 	20
#define SETTASKRELEASE	21


//Services definition
#define	PATH_CONNECTION_REQUEST		1000
#define	PATH_CONNECTION_RELEASE		1001
#define PATH_CONNECTION_ACK			1002
#define	NI_STATUS_REQUEST			1003
#define NI_STATUS_RESPONSE			1004

//Kernel Mode addess
#define	TO_KERNEL					0x10000

extern int SystemCall();

#define RequestServiceMode()				SystemCall(REQSERVICEMODE, 0, 0, 0)
#define SendService(target, msg, uint_size) while(!SystemCall(WRITESERVICE, target, (unsigned int *)msg, uint_size))
#define ReceiveService(msg)					while(!SystemCall(READSERVICE, 	(unsigned int *)msg, 0, 0))
#define Puts(str) 							while(!SystemCall(PUTS, 		(char*)str,			0, 0))
#define ConfigRouter(target, ports, subnet)	while(!SystemCall(CFGROUTER, 	target, ports, subnet))
#define	NoCSendFree()						SystemCall(NOCSENDFREE, 	0, 0, 0)
#define IncomingPacket()					SystemCall(INCOMINGPACKET, 	0, 0, 0)
#define GetNetAddress()						SystemCall(GETNETADDRESS, 	0, 0, 0)
#define AddTaskLocation(task_id, location)	SystemCall(ADDTASKLOCATION, task_id, location, 0)
#define RemoveTaskLocation(task_id)			SystemCall(REMOVETASKLOCATION, task_id, 0, 0)
#define GetTaskLocation(task_id)			SystemCall(GETTASKLOCATION, task_id, 0, 0)
#define SetMyID(new_id)						SystemCall(SETMYID, new_id, 0, 0)
#define SetTaskRelease(message, size)		SystemCall(SETTASKRELEASE, message, size, 0)


#endif /* _SERVICE_API_H_ */
