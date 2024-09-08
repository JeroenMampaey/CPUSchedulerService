#include "os_management_task.h"
#include "../../cpp_lib/placement_new.h"
#include "api_handlers.h"
#include "../global_resources/serial_log.h"
#include "../global_resources/screen.h"

/*
    Protocol for starting new processes uses CoAP:

    PUT /tasks/{id} for creating a new task
    GET /tasks/{id}/state returns "Running" or "Not Running"
    PUT /tasks/{id}/data/{address} [data], sets data at given address for task, uses octet-stream payload
    PUT /tasks/{id}/state [state], state can only be "Running" to change state to running, uses plain-text payload
    DELETE /tasks/{id} to delete task
*/

void osManagementTask(SocketManager* pSocketManager, MemoryManager* pMemoryManager){
    unsigned char* coapServerAddr = pMemoryManager->allocate(alignof(CoAPServer), sizeof(CoAPServer));
    if(coapServerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for CoAPServer in OS management task\n");
        return;
    }
    CoAPServer* pCoAPServer = new((unsigned char*)coapServerAddr) CoAPServer(COAP_UDP_PORT);

    unsigned char* taskManagerAddr = pMemoryManager->allocate(alignof(TaskManager), sizeof(TaskManager));
    if(taskManagerAddr == nullptr){
        Screen* pScreen = Screen::getScreen();
        pScreen->printk((char*)"Failed to allocate memory for TaskManager in OS management task\n");
        return;
    }
    TaskManager* pTaskManager = new((unsigned char*)taskManagerAddr) TaskManager();
    
    TaskAPIHandler taskHandler(pTaskManager);
    TaskStateAPIHandler taskStateHandler(pTaskManager);
    TaskDataAPIHandler taskDataHandler(pTaskManager);

    pCoAPServer->setPathHandler((char*)"/tasks/^", &taskHandler);
    pCoAPServer->setPathHandler((char*)"/tasks/^/state", &taskStateHandler);
    pCoAPServer->setPathHandler((char*)"/tasks/^/data/^", &taskDataHandler);

    #if E2E_TESTING
    SerialLog* pSerialLog = SerialLog::getSerialLog();
    pSerialLog->log((char*)"OSMgmt: starting CoAPServer run()\n");
    #endif

    pCoAPServer->run();
}