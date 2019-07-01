/*
 * connection_request.h
 *
 *  Created on: 7 de dez de 2018
 *      Author: mruaro
 */

#ifndef CONNECTION_REQUEST_H_
#define CONNECTION_REQUEST_H_

enum ReqMode { ESTABLISH, RELEASE};

typedef struct{
	unsigned int source;
	unsigned int target;
	unsigned int requester_address;
	int subnet;	//If subnet id is -1 then is the establishment mode, otherwise release mode
}ConnectionRequest;


#endif /* CONNECTION_REQUEST_H_ */
