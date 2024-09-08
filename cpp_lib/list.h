#pragma once

template<class T>
class LinkedListElement{
    public:
        T value;
        LinkedListElement* next;
};

template<class T>
class DoublyLinkedListElement{
    public:
        T value;
        DoublyLinkedListElement* next;
        DoublyLinkedListElement* prev;
};

template<unsigned int numNextValues, class T>
class MultiLinkedListElement{
    public:
        T value;
        MultiLinkedListElement* next[numNextValues];
};