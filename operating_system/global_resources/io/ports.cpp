#include "ports.h"

unsigned char portByteIn(unsigned short port){
	unsigned char result;
	/*
	copy value from IO-port specified by dx into al, 
	put value from al into result
	*/
	__asm__("in %%dx, %%al" : "=a" (result) : "d" (port) : "memory");
	return result;
}

void portByteOut(unsigned short port, unsigned char data){
	/*
	 put data into al en port into dx and
	 out al into the IO-port specified by dx 
	 */
	__asm__ __volatile__("out %%al, %%dx" : : "a" (data), "d" (port) : "memory");
}

unsigned short portWordIn(unsigned short port){
	unsigned short result;
	/*similar to port_byte_in*/
	__asm__("in %%dx, %%ax" : "=a" (result) : "d" (port) : "memory");
	return result;
}

void portWordOut(unsigned short port, unsigned short data){
	/*similar to port_byte_out*/
	__asm__ __volatile__("out %%ax, %%dx" : : "a" (data), "d" (port) : "memory");
}

unsigned int portDwordIn(unsigned short port){
	unsigned int result;
	/*similar to port_byte_in*/
	__asm__("in %%dx, %%eax" : "=a" (result) : "d" (port) : "memory");
	return result;
}

void portDwordOut(unsigned short port, unsigned int data){
	/*similar to port_byte_out*/
	__asm__ __volatile__("out %%eax, %%dx" : : "a" (data), "d" (port) : "memory");
}