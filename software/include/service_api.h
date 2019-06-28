/*
 * os_service_api.h
 *
 *  Created on: 28/09/2016
 *      Author: mruaro
 */

#ifndef _SERVICE_API_H_
#define _SERVICE_API_H_

//These macros must be a continuation of macros present into api.h
#define	REQSERVICEMODE	7
#define WRITESERVICE   	8
#define READSERVICE     9
#define	PUTS			10
#define CFGROUTER		11
#define NOCSENDFREE		12
#define INCOMINGPACKET	13

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
#define SendService(target, msg, uint_size) while(!SystemCall(WRITESERVICE, target, (unsigned int *)msg, uint_size, 0))
#define ReceiveService(msg)					while(!SystemCall(READSERVICE, 	(unsigned int *)msg, 0, 0))
#define Puts(str) 							while(!SystemCall(PUTS, 		(char*)str,			0, 0))
#define ConfigRouter(target, ports, subnet)	while(!SystemCall(CFGROUTER, 	target, ports, subnet))
#define	NoCSendFree()						SystemCall(NOCSENDFREE, 	0, 0, 0)
#define IncomingPacket()					SystemCall(INCOMINGPACKET, 	0, 0, 0)

#endif /* _SERVICE_API_H_ */
