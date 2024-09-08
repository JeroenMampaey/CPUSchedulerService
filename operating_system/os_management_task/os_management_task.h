#pragma once

#include "../network_management_task/socket_manager.h"
#include "../../cpp_lib/coap_server.h"
#include "task_manager.h"
#include "../network_management_task/network_management_task.h"
#include "../../cpp_lib/memory_manager.h"

#define COAP_UDP_PORT 5683

void osManagementTask(SocketManager* pSocketManager, MemoryManager* pMemoryManager);