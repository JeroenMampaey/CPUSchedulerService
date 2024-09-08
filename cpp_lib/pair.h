#pragma once

template<class T, class V>
class Pair{
    public:
        T first;
        V second;

        Pair() {}
        Pair(T first, V second) : first(first), second(second) {}  
};