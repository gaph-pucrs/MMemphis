/*
 * security.h
 *
 *  Created on: 25 de mar de 2020
 *      Author: ruaro
 */

#ifndef APPLICATIONS_MAN_APP_MAPPING_INCLUDES_GLOBAL_MAPPER_SECURITY_H_
#define APPLICATIONS_MAN_APP_MAPPING_INCLUDES_GLOBAL_MAPPER_SECURITY_H_

#include <stdint.h>


#define UINT_KEY_SIZE	4

char key[16] = {0,1,2,3,   4,5,6,7,  8,9,0xa,0xb, 0xc,0xd,0xe,0xf};

unsigned int Ke[UINT_KEY_SIZE] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
unsigned int Rnd[UINT_KEY_SIZE] = {0xBE105, 0x5E105, 0xF0DA, 0xEB0A};

void gen_rnd(){
}


/************************************ csiphash **************************************/

/* See: http://sourceforge.net/p/predef/wiki/Endianness/ */
#  if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#    include <sys/endian.h>
#  else
#    include <machine/endian.h>
#  endif

#  if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
	__BYTE_ORDER == __LITTLE_ENDIAN
#    define _le64toh(x) ((uint64_t)(x))
#  else
#    define _le64toh(x) le64toh(x)
#  endif

#define ROTATE(x, b) (uint64_t)( ((x) << (b)) | ( (x) >> (64 - (b))) )

#define HALF_ROUND(a,b,c,d,s,t)			\
	a += b; c += d;				\
	b = ROTATE(b, s) ^ a;			\
	d = ROTATE(d, t) ^ c;			\
	a = ROTATE(a, 32);

#define DOUBLE_ROUND(v0,v1,v2,v3)		\
	HALF_ROUND(v0,v1,v2,v3,13,16);		\
	HALF_ROUND(v2,v1,v0,v3,17,21);		\
	HALF_ROUND(v0,v1,v2,v3,13,16);		\
	HALF_ROUND(v2,v1,v0,v3,17,21);


uint64_t le64toh(uint64_t x);
uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16]) ;


uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16]) {
	const uint64_t *_key = (uint64_t *)key;
	uint64_t k0 = _le64toh(_key[0]);
	uint64_t k1 = _le64toh(_key[1]);
	uint64_t b = (uint64_t)src_sz << 56;
	const uint64_t *in = (uint64_t*)src;

	uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
	uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
	uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
	uint64_t v3 = k1 ^ 0x7465646279746573ULL;

	while (src_sz >= 8) {
		uint64_t mi = _le64toh(*in);
		in += 1; src_sz -= 8;
		v3 ^= mi;
		DOUBLE_ROUND(v0,v1,v2,v3);
		v0 ^= mi;
	}

	uint64_t t = 0; uint8_t *pt = (uint8_t *)&t; uint8_t *m = (uint8_t *)in;
	switch (src_sz) {
	case 7: pt[6] = m[6];
	case 6: pt[5] = m[5];
	case 5: pt[4] = m[4];
	case 4: *((uint32_t*)&pt[0]) = *((uint32_t*)&m[0]); break;
	case 3: pt[2] = m[2];
	case 2: pt[1] = m[1];
	case 1: pt[0] = m[0];
	}
	b |= _le64toh(t);

	v3 ^= b;
	DOUBLE_ROUND(v0,v1,v2,v3);
	v0 ^= b; v2 ^= 0xff;
	DOUBLE_ROUND(v0,v1,v2,v3);
	DOUBLE_ROUND(v0,v1,v2,v3);
	return (v0 ^ v1) ^ (v2 ^ v3);
}

uint64_t le64toh(uint64_t x){
	uint64_t dest;

	dest =  (((x >> 56) & 0xFF) <<  0) |
			(((x >> 48) & 0xFF) <<  8) |
			(((x >> 40) & 0xFF) << 16) |
			(((x >> 32) & 0xFF) << 24) |

			(((x >> 24) & 0xFF) << 32) |
			(((x >> 16) & 0xFF) << 40) |
			(((x >>  8) & 0xFF) << 48) |
			(((x >>  0) & 0xFF) << 56) ;

	return dest;
}

/************************************ csiphash **************************************/

void send_key(unsigned int * key, unsigned int service, unsigned int target_slave, unsigned int app_id){
	unsigned int * message;
	int msg_size = 0, key_int_size=0, value;

	message = get_message_slot();
	message[0] = target_slave; //flit 0 header
	message[1] = 3; 		   //flit 1 payload size
	message[2] = service;	   //flit 2 service
	message[3] = app_id;	   //flit 3 task_ID

	msg_size = 13; //CONSTANT_PKT_SIZE defined into packet.h
	for(int i=0; i<UINT_KEY_SIZE; i++){
		message[msg_size++] = key[i];
		key_int_size++;
	}

	message[1] = msg_size-2; //Packet payload size
	message[8] = key_int_size;  //flit 8 msg_lenght

	SendRaw(message, msg_size);
	while(!NoCSendFree());
}



void manage_secure_allocation(unsigned int app_id, unsigned int alloc_proc){
	uint64_t Km_output;
	unsigned int M[UINT_KEY_SIZE];
	unsigned int Km[UINT_KEY_SIZE];



	//Send Rnd
	send_key(Rnd, RND_MESSAGE, alloc_proc, app_id);
	//Puts("Sent RND to "); Puts(itoh(alloc_proc)); Puts("\n");


	Km_output = siphash24((void *)Rnd, UINT_KEY_SIZE*4, Rnd);
	unsigned int  Km_lo, Km_hi;
	Km_hi = Km_output >> 32;
	Km_lo = Km_output & 0xFFFFFFFF;

	/*Puts("Km: ");  Puts(itoh(Km_output));  Puts("\n");
	Puts("Calculated Km hi:"); Puts(itoh(Km_hi)); Puts("\n");
	Puts("Calculated Km lo:"); Puts(itoh(Km_lo)); Puts("\n");*/

	//Converts the 64 bits of Km_output to a char array of 16 elements

	Km[0] = Km_hi;
	Km[1] = Km_lo;
	Km[2] = Km_hi;
	Km[3] = Km_lo;


	/*Puts("KM = {");
	for(int i=0; i<UINT_KEY_SIZE; i++){
		Puts(itoh(Km[i])); Puts(", ");
	}
	Puts("}\n");

	Puts("Ke = {");
	for(int i=0; i<UINT_KEY_SIZE; i++){
		Puts(itoh(Ke[i])); Puts(", ");
	}
	Puts("}\n");*/

	//M = Km XOR Ke
	for(int i=0; i<UINT_KEY_SIZE; i++){
		M[i] = Km[i] ^ Ke[i];
	}

	/*Puts("M = {");
	for(int i=0; i<UINT_KEY_SIZE; i++){
		Puts(itoh(M[i])); Puts(", ");
	}
	Puts("}\n");*/

	//Send M
	send_key(M, M_MESSAGE, alloc_proc, app_id);
	//Puts("Sent M to "); Puts(itoh(alloc_proc)); Puts("\n");

}


#endif /* APPLICATIONS_MAN_APP_MAPPING_INCLUDES_GLOBAL_MAPPER_SECURITY_H_ */
