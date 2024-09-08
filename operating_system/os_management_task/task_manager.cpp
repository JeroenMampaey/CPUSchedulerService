#include "task_manager.h"
#include "../cpp/placement_new.h"

#define VIRTUAL_TASK_SPACE_BEGIN_ADDR 0x2000000

TaskManager::TaskManager()
    :
    activeTasksLinkedListHead(nullptr),
    unusedTaskLinkedListElementsHead(nullptr)
{
    for(unsigned int i = 0; i < TASK_MANAGER_MAX_TASKS; i++){
        taskLinkedListElements[i].value = {i, 0};
        if(i == TASK_MANAGER_MAX_TASKS-1){
            taskLinkedListElements[i].next = nullptr;
        }
        else{
            taskLinkedListElements[i].next = &taskLinkedListElements[i+1];
        }
    }
    unusedTaskLinkedListElementsHead = &taskLinkedListElements[0];
}

TaskManager::~TaskManager(){
    LinkedListElement<Pair<unsigned int, unsigned int>>* currentElement = activeTasksLinkedListHead;
    while(currentElement != nullptr){
        CpuCore::UserTask* userTask = (CpuCore::UserTask*)userTaskBuffers[currentElement->value.first].buffer;
        userTask->~UserTask();

        LinkedListElement<Pair<unsigned int, unsigned int>>* nextElement = currentElement->next;
        currentElement = nextElement;
    }
}

CreateUserTaskResult TaskManager::createUserTask(CpuCore* pCpuCore, unsigned int id){
    if(id == 0){
        return CreateUserTaskResult::ID_IS_NULL;
    }

    if(activeTasksLinkedListHead != nullptr){
        LinkedListElement<Pair<unsigned int, unsigned int>>* currentElement = activeTasksLinkedListHead;
        while(currentElement != nullptr){
            if(currentElement->value.second == id){
                return CreateUserTaskResult::ID_ALREADY_EXISTS;
            }
            currentElement = currentElement->next;
        }
    }

    if(unusedTaskLinkedListElementsHead == nullptr){
        return CreateUserTaskResult::TOO_MANY_TASKS;
    }

    LinkedListElement<Pair<unsigned int, unsigned int>>* newElement = unusedTaskLinkedListElementsHead;
    unusedTaskLinkedListElementsHead = unusedTaskLinkedListElementsHead->next;

    newElement->value.second = id;
    newElement->next = activeTasksLinkedListHead;
    activeTasksLinkedListHead = newElement;

    CpuCore::UserTask* userTask = new(userTaskBuffers[newElement->value.first].buffer) CpuCore::UserTask(pCpuCore, VIRTUAL_TASK_SPACE_BEGIN_ADDR);

    return CreateUserTaskResult::SUCCESS;
}

CpuCore::UserTask* TaskManager::getUserTask(unsigned int id){
    LinkedListElement<Pair<unsigned int, unsigned int>>* currentElement = activeTasksLinkedListHead;
    while(currentElement != nullptr){
        if(currentElement->value.second == id){
            return (CpuCore::UserTask*)userTaskBuffers[currentElement->value.first].buffer;
        }
        currentElement = currentElement->next;
    }

    return nullptr;
}

RemoveUserTaskResult TaskManager::removeUserTask(unsigned int id){
    if(id == 0){
        return RemoveUserTaskResult::ID_IS_NULL;
    }

    LinkedListElement<Pair<unsigned int, unsigned int>>* previousElement = nullptr;
    LinkedListElement<Pair<unsigned int, unsigned int>>* currentElement = activeTasksLinkedListHead;
    while(currentElement != nullptr){
        if(currentElement->value.second == id){
            if(previousElement == nullptr){
                activeTasksLinkedListHead = currentElement->next;
            }
            else{
                previousElement->next = currentElement->next;
            }

            currentElement->next = unusedTaskLinkedListElementsHead;
            unusedTaskLinkedListElementsHead = currentElement;

            CpuCore::UserTask* userTask = (CpuCore::UserTask*)userTaskBuffers[currentElement->value.first].buffer;
            userTask->~UserTask();

            return RemoveUserTaskResult::SUCCESS;
        }
        previousElement = currentElement;
        currentElement = currentElement->next;
    }

    return RemoveUserTaskResult::ID_DOES_NOT_EXIST;
}