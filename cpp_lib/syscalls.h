#pragma once

#define RECEIVE_BUFFER_HEADER_SIZE (4 + 2 + 2)
#define SEND_BUFFER_HEADER_SIZE (4 + 2 + 2)

#define UDP_HEADER_SIZE 8

void yield();

// Returns -1 for failure, otherwise returns the socket ID
// Fails if udpPort is already in use (or if udpPort is bigger than NUM_UDP_PORTS defined in socket_manager.h) 
// or if the task has already opened too many sockets
int openSocket(unsigned short udpPort);

// Returns -1 for failure, otherwise returns 0
// Only fails if socketID does not point to an open socket or if the buffer is not valid
int setReceiveBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize);

// Returns -1 for failure, otherwise returns 0
// indicatorWhenFinished will be set to 0 and changed to 1 when the OS managed to write away the entire buffer
// Only fails if socketID does not point to an open socket or if the buffer/indicatorWhenFinished is not valid
int setSendBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize, int* indicatorWhenFinished);

// Doesn't return anything, if socketID points to an open socket then this socket will be closed
void closeSocket(unsigned char socketID);

void print(char* string);

// Returns a counter which is incremented ~20 times per second, don't expect it to be very exact though. 
// Value is 0 when the OS is started and wraps around like a normal unsigned int. 
// Because of task switching, also don't expect to see every tick.
unsigned int getTimerCounter();

// This only works when operating system is compiled with flag E2ETESTING=1
void e2eTestingLog(int loggedValue);