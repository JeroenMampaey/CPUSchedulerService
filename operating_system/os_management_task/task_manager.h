#pragma once

#include "../../cpp_lib/list.h"
#include "../../cpp_lib/pair.h"
#include "../cpu_core/cpu_core.h"

#define TASK_MANAGER_MAX_TASKS (2*NUM_POSSIBLE_TASKS)

typedef struct UserTaskBuffer{
    unsigned char buffer[sizeof(CpuCore::UserTask)];
} UserTaskBuffer __attribute__((aligned(alignof(CpuCore::UserTask))));

enum class CreateUserTaskResult{
    SUCCESS,
    ID_IS_NULL,
    ID_ALREADY_EXISTS,
    TOO_MANY_TASKS
};

enum class RemoveUserTaskResult{
    SUCCESS,
    ID_IS_NULL,
    ID_DOES_NOT_EXIST
};

class TaskManager{
    private:
        UserTaskBuffer userTaskBuffers[TASK_MANAGER_MAX_TASKS];

        LinkedListElement<Pair<unsigned int, unsigned int>> taskLinkedListElements[TASK_MANAGER_MAX_TASKS];
        LinkedListElement<Pair<unsigned int, unsigned int>>* activeTasksLinkedListHead;
        LinkedListElement<Pair<unsigned int, unsigned int>>* unusedTaskLinkedListElementsHead;
    
    public:
        TaskManager();
        ~TaskManager();

        CreateUserTaskResult createUserTask(CpuCore* pCpuCore, unsigned int id);
        CpuCore::UserTask* getUserTask(unsigned int id);
        RemoveUserTaskResult removeUserTask(unsigned int id);
};