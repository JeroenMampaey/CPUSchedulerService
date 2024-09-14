#pragma once

#define RECEIVE_BUFFER_HEADER_SIZE (4 + 2 + 2)
#define SEND_BUFFER_HEADER_SIZE (4 + 2 + 2)

#define UDP_HEADER_SIZE 8

/*
    Allow the OS to switch to the next task
*/
void yield();

/*
    Open a UDP socket on a specific port

    Returns -1 for failure, otherwise returns the socket ID which is castable to an unsigned char

    When does failure occur?
        - If the UDP port is already in use by this or another task
        - If the UDP port number is bigger than NUM_UDP_PORTS defined in socket_manager.h
        - If the task has already opened too many sockets, limit is defined by MAX_NUM_SOCKETS_PER_TASK in socket_manager.h
*/
int openSocket(unsigned short udpPort);

/*
    Set a buffer for receiving data

    Returns -1 for failure, otherwise returns 0

    When does failure occur?
        - If the socketID does not point to an open socket
        - If the buffer is not in the task accessible space
        - If the bufferSize is smaller than 2*RECEIVE_BUFFER_HEADER_SIZE
    
    However!:
        Buffer==nullptr and bufferSize==0 is valid input, this will tell the OS that the task is not interested in receiving data
*/
int setReceiveBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize);

/*
    Send data from the buffer from the socket port

    Returns -1 for failure, otherwise returns 0

    The indicatorWhenFinished will be set to 0 and changed to 1 when the OS managed to send the entire buffer over the network

    When does failure occur?
        - If the socketID does not point to an open socket
        - If the buffer is not in the task accessible space
        - If the bufferSize is smaller than SEND_BUFFER_HEADER_SIZE+UDP_HEADER_SIZE
        - If the OS has already too many transmission requests, limit is defined by MAX_NUM_TRANSMISSION_REQUESTS in socket_manager.h
        - If the indicatorWhenFinished is not in the task accessible space
    
    However!:
        Buffer==nullptr and bufferSize==0 and indicatorWhenFinished==nullptr is valid input, this will tell the OS that the task is not interested in sending data

    If setSendBuffer was a success, the buffer should not be modified until the indicatorWhenFinished is set to 1
    indicatorWhenFinished is guaranteed to become 1 given sufficient time
    
    It is however possible to call setSendBuffer again before the indicatorWhenFinished is set to 1
    ->
    If successfull, the previous buffer becomes usable again but might not have been send over the network
    ->
    setSendBuffer(socketID, nullptr, 0, nullptr) can be used to cancel the previous transmission request
*/
int setSendBuffer(unsigned char socketID, unsigned char* buffer, unsigned int bufferSize, int* indicatorWhenFinished);

/*
    Close a socket

    No return value, if the socketID points to an open socket then this socket will be closed
*/
void closeSocket(unsigned char socketID);

/*
    Print a string to the screen

    No return value, if the string is not in the task accessible space then the OS will ignore this call
*/
void print(char* string);

/*
    Get the timer counter

    Returns the timer counter, this is a value which is initialized to 0 when the OS is started and is incremented ~20 times per second

    Like a normal unsigned int, it wraps around when it reaches the maximum value
*/
unsigned int getTimerCounter();

/*
    Log a value for end-to-end testing

    No return value, if the OS is not compiled with E2ETESTING=1 then this call will not work

    This syscall really shouldn't be used except during the end-to-end tests 
*/
void e2eTestingLog(int loggedValue);