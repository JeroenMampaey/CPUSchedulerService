#include "api_handlers.h"

#include "../../cpp_lib/mem.h"
#include "../../cpp_lib/string.h"

TaskAPIHandler::TaskAPIHandler(TaskManager* pTaskManager)
    :
    pTaskManager(pTaskManager)
{}

CoAPResponse TaskAPIHandler::handlePUT(char* path,
    ContentFormat contentFormat,
    unsigned char* payload, 
    unsigned int payloadSize, 
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE])
{
    unsigned long long taskId = 0;
    // /tasks/{id} -> id starts at index 7
    for(int i=7; i<strlen(path); i++){
        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        taskId = taskId*10 + (path[i]-'0');

        if(taskId>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Task ID cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    // Only one CPU core is supported for now, thus all tasks will be scheduled there
    CpuCore* pCpuCore = CpuCore::getCpuCore(0);
    CreateUserTaskResult result = pTaskManager->createUserTask(pCpuCore, taskId);

    CoAPResponse response;
    switch(result){
        case CreateUserTaskResult::SUCCESS:
            response.responseCode = CREATED_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case CreateUserTaskResult::ID_IS_NULL:
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            {
                char* responseString = (char*)"Task ID cannot be 0";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case CreateUserTaskResult::ID_ALREADY_EXISTS:
            response.responseCode = CONFLICT_RESPONSE_CODE;
            {
                char* responseString = (char*)"Task with the given ID already exists";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case CreateUserTaskResult::TOO_MANY_TASKS:
            response.responseCode = FORBIDDEN_RESPONSE_CODE;
            {
                char* responseString = (char*)"Too many tasks have already been created, remove some tasks before creating new ones";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        default:
            response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
    }
}

CoAPResponse TaskAPIHandler::handleDELETE(char* path,
    ContentFormat contentFormat,
    unsigned char* payload, 
    unsigned int payloadSize, 
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE])
{
    unsigned long long taskId = 0;
    // /tasks/{id} -> id starts at index 7
    for(int i=7; i<strlen(path); i++){
        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        taskId = taskId*10 + (path[i]-'0');

        if(taskId>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Task ID cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    RemoveUserTaskResult result = pTaskManager->removeUserTask(taskId);

    CoAPResponse response;
    switch(result){
        case RemoveUserTaskResult::SUCCESS:
            response.responseCode = DELETED_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case RemoveUserTaskResult::ID_IS_NULL:
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            {
                char* responseString = (char*)"Task ID cannot be 0";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case RemoveUserTaskResult::ID_DOES_NOT_EXIST:
            response.responseCode = NOT_FOUND_RESPONSE_CODE;
            {
                char* responseString = (char*)"Task with the given ID does not exist";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        default:
            response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
    }
}

TaskStateAPIHandler::TaskStateAPIHandler(TaskManager* pTaskManager)
    :
    pTaskManager(pTaskManager)
{}

CoAPResponse TaskStateAPIHandler::handleGET(char* path,
    ContentFormat contentFormat,
    unsigned char* payload, 
    unsigned int payloadSize, 
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE])
{
    unsigned long long taskId = 0;
    // /tasks/{id}/state -> id starts at index 7
    for(int i=7; i<strlen(path); i++){
        if(path[i]=='/'){
            if(i==7){
                CoAPResponse response;
                response.responseCode = BAD_REQUEST_RESPONSE_CODE;
                response.responseSize = 0;
                response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
                return response;
            }

            break;
        }

        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        taskId = taskId*10 + (path[i]-'0');

        if(taskId>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Task ID cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    CpuCore::UserTask* pUserTask = pTaskManager->getUserTask(taskId);

    if(pUserTask == nullptr){
        CoAPResponse response;
        response.responseCode = NOT_FOUND_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    if(pUserTask->getRunningState()){
        char* responseString = (char*)"Running";
        CoAPResponse response;
        response.responseCode = CONTENT_RESPONSE_CODE;
        response.responseSize = strlen(responseString);
        memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }
    else{
        char* responseString = (char*)"Not Running";
        CoAPResponse response;
        response.responseCode = CONTENT_RESPONSE_CODE;
        response.responseSize = strlen(responseString);
        memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }
}

CoAPResponse TaskStateAPIHandler::handlePUT(char* path,
    ContentFormat contentFormat,
    unsigned char* payload, 
    unsigned int payloadSize, 
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE])
{
    volatile unsigned int* packetCallback = (volatile unsigned int*)0x201D9E8;
    if(contentFormat!=ContentFormat::Text_Plain_Charset_UTF8){
        CoAPResponse response;
        response.responseCode = UNSUPPORTED_CONTENT_FORMAT_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    // "Running" is the only allowed payload
    if(payloadSize!=7 || memCompare(payload, (unsigned char*)"Running", 7)!=0){
        CoAPResponse response;
        response.responseCode = BAD_REQUEST_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    unsigned long long taskId = 0;
    // /tasks/{id}/state -> id starts at index 7
    for(int i=7; i<strlen(path); i++){
        if(path[i]=='/'){
            if(i==7){
                CoAPResponse response;
                response.responseCode = BAD_REQUEST_RESPONSE_CODE;
                response.responseSize = 0;
                response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
                return response;
            }

            break;
        }

        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        taskId = taskId*10 + (path[i]-'0');

        if(taskId>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Task ID cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    CpuCore::UserTask* pUserTask = pTaskManager->getUserTask(taskId);

    if(pUserTask == nullptr){
        CoAPResponse response;
        response.responseCode = NOT_FOUND_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    if(pUserTask->getRunningState()){
        CoAPResponse response;
        response.responseCode = CHANGED_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    TaskArguments emptyTaskArguments;
    emptyTaskArguments.numArgs = 0;
    pUserTask->setTaskArguments(emptyTaskArguments);

    SetToRunningStateResponse setResponse = pUserTask->setToRunningState();
    CoAPResponse response;
    switch(setResponse){
        case SetToRunningStateResponse::TASK_ALREADY_RUNNING:
            response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case SetToRunningStateResponse::TASK_NOT_PROPERLY_INITIALIZED:
            response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case SetToRunningStateResponse::TOO_MANY_TASKS:
            response.responseCode = FORBIDDEN_RESPONSE_CODE;
            {
                char* responseString = (char*)"Too many tasks are already running";
                response.responseSize = strlen(responseString);
                memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            }
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        case SetToRunningStateResponse::SUCCESS:
            response.responseCode = CHANGED_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        default:
            response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
    }
}

TaskDataAPIHandler::TaskDataAPIHandler(TaskManager* pTaskManager)
    :
    pTaskManager(pTaskManager)
{}

CoAPResponse TaskDataAPIHandler::handlePUT(char* path,
    ContentFormat contentFormat,
    unsigned char* payload, 
    unsigned int payloadSize, 
    unsigned char responseBuffer[RESPONSE_BUFFER_SIZE])
{
    if(contentFormat!=ContentFormat::Application_Octet_Stream){
        CoAPResponse response;
        response.responseCode = UNSUPPORTED_CONTENT_FORMAT_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    unsigned long long taskId = 0;
    // /tasks/{id}/data/{address} -> id starts at index 7
    int i=7;
    for(; i<strlen(path); i++){
        if(path[i]=='/'){
            if(i==7){
                CoAPResponse response;
                response.responseCode = BAD_REQUEST_RESPONSE_CODE;
                response.responseSize = 0;
                response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
                return response;
            }

            break;
        }

        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        taskId = taskId*10 + (path[i]-'0');

        if(taskId>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Task ID cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    unsigned long long address = 0;
    // /tasks/{id}/data/{address} -> now we are at the index of the second '/', address starts 6 characters after this
    i += 6;
    if(i>=strlen(path)){
        CoAPResponse response;
        response.responseCode = BAD_REQUEST_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }
    for(; i<strlen(path); i++){
        if(path[i]<'0' || path[i]>'9'){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            response.responseSize = 0;
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }

        address = address*10 + (path[i]-'0');

        if(address>0xFFFFFFFF){
            CoAPResponse response;
            response.responseCode = BAD_REQUEST_RESPONSE_CODE;
            char* responseString = (char*)"Address cannot be greater than 0xFFFFFFFF";
            response.responseSize = strlen(responseString);
            memCopy((unsigned char*)responseString, responseBuffer, response.responseSize);
            response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
            return response;
        }
    }

    CpuCore::UserTask* pUserTask = pTaskManager->getUserTask(taskId);
    if(pUserTask == nullptr){
        CoAPResponse response;
        response.responseCode = NOT_FOUND_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    if(!pUserTask->addrSpaceIsUserAccessible(address, payloadSize)){
        CoAPResponse response;
        response.responseCode = FORBIDDEN_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    if(payloadSize==0){
        CoAPResponse response;
        response.responseCode = CHANGED_RESPONSE_CODE;
        response.responseSize = 0;
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    bool success = pUserTask->setData(address, payload, payloadSize);
    if(!success){
        CoAPResponse response;
        response.responseCode = INTERNAL_SERVER_ERROR_RESPONSE_CODE;
        unsigned char* responseString = (unsigned char*)"Failed to write data to the task's address space";
        response.responseSize = strlen((char*)responseString);
        memCopy(responseString, responseBuffer, response.responseSize);
        response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
        return response;
    }

    CoAPResponse response;
    response.responseCode = CHANGED_RESPONSE_CODE;
    if(pUserTask->getRunningState()){
        unsigned char* responseString = (unsigned char*)"Warning: data changed while task was running";
        response.responseSize = strlen((char*)responseString);
        memCopy(responseString, responseBuffer, response.responseSize);
    }
    else{
        response.responseSize = 0;
    }
    response.contentFormat = ContentFormat::Text_Plain_Charset_UTF8;
    return response;
}