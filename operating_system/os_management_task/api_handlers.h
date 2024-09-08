#pragma once

#include "../../cpp_lib/coap_server.h"
#include "task_manager.h"

class TaskAPIHandler : public CoAPHandler{
    private:
        TaskManager* pTaskManager;

    public:
        TaskAPIHandler(TaskManager* pTaskManager);

        CoAPResponse handlePUT(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override;
        
        CoAPResponse handleDELETE(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override;
};

class TaskStateAPIHandler : public CoAPHandler{
    private:
        TaskManager* pTaskManager;

    public:
        TaskStateAPIHandler(TaskManager* pTaskManager);

        CoAPResponse handleGET(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override;
        
        CoAPResponse handlePUT(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override;
};

class TaskDataAPIHandler : public CoAPHandler{
    private:
        TaskManager* pTaskManager;

    public:
        TaskDataAPIHandler(TaskManager* pTaskManager);

        CoAPResponse handlePUT(char* path,
            ContentFormat contentFormat,
            unsigned char* payload, 
            unsigned int payloadSize, 
            unsigned char responseBuffer[RESPONSE_BUFFER_SIZE]) override;
};