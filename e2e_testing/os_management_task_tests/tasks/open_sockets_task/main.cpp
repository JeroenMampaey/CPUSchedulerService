#include "../../../../cpp_lib/syscalls.h"

void main(){
    int socketID1 = openSocket(123);
    int socketID2 = openSocket(400);
    int socketID3 = openSocket(1000);
    int socketID4 = openSocket(8000);

    if(socketID1!=-1 && socketID2!=-1 && socketID3!=-1 && socketID4!=-1){
        e2eTestingLog(123);
    }
    else if(socketID1==-1 && socketID2==-1 && socketID3==-1 && socketID4==-1){
        e2eTestingLog(456);
    }
    else{
        e2eTestingLog(789);
    }

    while(1){
        yield();
    }
}