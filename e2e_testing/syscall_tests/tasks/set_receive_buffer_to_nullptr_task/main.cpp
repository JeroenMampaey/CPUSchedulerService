#include "../../../../cpp_lib/syscalls.h"

void main(){
    int socketID = openSocket(1000);

    if(socketID != -1){
        // First, test setting an actual receive buffer
        unsigned char receiveBuffer[100];
        int success = setReceiveBuffer(socketID, receiveBuffer, 100);
        
        // Normally, this should succeed
        if(success==-1){
            e2eTestingLog(88);
        }
        else{
            e2eTestingLog(89);
        }

        // Next, test setting a nullptr/null size receive buffer
        success = setReceiveBuffer(socketID, nullptr, 0);

        // Normally, this should succeed
        if(success==-1){
            e2eTestingLog(90);
        }
        else{
            e2eTestingLog(91);
        }

        // Next, test setting a nullptr/non-zero size receive buffer
        success = setReceiveBuffer(socketID, nullptr, 100);

        // Normally, this should fail
        if(success==-1){
            e2eTestingLog(92);
        }
        else{
            e2eTestingLog(93);
        }

        // Next, test setting a non-nullptr/zero size receive buffer
        success = setReceiveBuffer(socketID, receiveBuffer, 0);

        // Normally, this should fail
        if(success==-1){
            e2eTestingLog(94);
        }
        else{
            e2eTestingLog(95);
        }

        closeSocket(socketID);
    }

    while(1){
        yield();
    }
}