#pragma once

#include "socket_manager.h"
#include "network_stack_handler.h"
#include "../cpu_core/cpu_core.h"
#include "../../cpp_lib/memory_manager.h"

void networkManagementTask(CpuCore* pThisCpuCore, SocketManager* pSocketManager, MemoryManager* pMemoryManager);