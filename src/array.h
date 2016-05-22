#ifndef H_HPLANG_ARRAY_H

#include "types.h"
#include "memory.h"
#include "assert.h"

#include <cstring>

namespace hplang
{

// Array for POD-types
template <class T>
struct Array
{
    s64 capacity;
    s64 count;
    T *data;
    
    T& operator [] (s64 index);
    const T& operator [] (s64 index) const;
};

template <class T>
T& Array<T>::operator [] (s64 index)
{
    ASSERT(0 <= index && index < count);
    return data[index];
}

template <class T>
const T& Array<T>::operator [] (s64 index) const
{
    ASSERT(0 <= index && index < count);
    return data[index];
}

namespace array
{
    template <class T>
    bool Reserve(Array<T> &arr, s64 capacity);

    template <class T>
    bool Resize(Array<T> &arr, s64 count);

    template <class T>
    void Clear(Array<T> &arr);

    template <class T>
    bool Push(Array<T> &arr, const T &x);

    template <class T>
    bool Insert(Array<T> &arr, s64 index, const T &x);

    template <class T>
    void Set(Array<T> &arr, s64 index, const T &x);

    template <class T>
    T At(Array<T> &arr, s64 index);

    template <class T>
    void EraseBySwap(Array<T> &arr, s64 index);

    template <class T>
    void Free(Array<T> &arr);
}

namespace array
{
    template <class T>
    bool Reserve(Array<T> &arr, s64 capacity)
    {
        if (arr.capacity >= capacity) return true;

        s64 old_capacity = arr.capacity;
        s64 new_capacity = capacity;

        Pointer data_p;
        data_p.ptr = arr.data;
        data_p.size = old_capacity * sizeof(T);
        data_p = Realloc(data_p, new_capacity * sizeof(T));
        if (data_p.ptr)
        {
            arr.capacity = new_capacity;
            arr.data = (T*)data_p.ptr;
            return true;
        }
        ASSERT(0 && "SHOULD NOT HAPPEN IN NORMAL USE");
        return false;
    }

    template <class T>
    bool Resize(Array<T> &arr, s64 count)
    {
        if (!Reserve(arr, count)) return false;

        s64 tail_count = count - arr.count;
        if (tail_count > 0)
        {
            memset(arr.data + arr.count, 0, tail_count * sizeof(T));
        }
        arr.count = count;
        return true;
    }

    template <class T>
    void Clear(Array<T> &arr)
    {
        arr.count = 0;
    }

    template <class T>
    bool Push(Array<T> &arr, const T &x)
    {
        if (arr.capacity <= arr.count)
        {
            s64 new_capacity = arr.capacity + arr.capacity / 2;
            if (new_capacity < 8) new_capacity = 8;
            if (!Reserve(arr, new_capacity))
                return false;
        }
        arr.data[arr.count++] = x;
        return true;
    }

    template <class T>
    bool Insert(Array<T> &arr, s64 index, const T &x)
    {
        if (arr.capacity <= arr.count)
        {
            s64 new_capacity = arr.capacity + arr.capacity / 2;
            if (new_capacity < 8) new_capacity = 8;
            if (!Reserve(arr, new_capacity))
                return false;
        }
        for (s64 i = arr.count; i > index; i--)
        {
            arr.data[i] = arr.data[i - 1];
        }
        arr.data[index] = x;
        arr.count++;
        return true;
    }

    template <class T>
    void Set(Array<T> &arr, s64 index, const T &x)
    {
        ASSERT(0 <= index && index < arr.count);
        arr.data[index] = x;
    }

    template <class T>
    T At(Array<T> &arr, s64 index)
    {
        ASSERT(0 <= index && index < arr.count);
        return arr.data[index];
    }

    template <class T>
    void EraseBySwap(Array<T> &arr, s64 index)
    {
        ASSERT(0 <= index && index < arr.count);
        if (index < arr.count - 1)
        {
            arr.data[index] = arr.data[arr.count - 1];
        }
        arr.count--;
    }

    template <class T>
    void Free(Array<T> &arr)
    {
        if (!arr.data) return;
        Pointer data_p;
        data_p.ptr = arr.data;
        data_p.size = arr.capacity * sizeof(T);
        Free(data_p);
    }
}

} // hplang

#define H_HPLANG_ARRAY_H
#endif
