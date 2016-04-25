#ifndef H_HPLANG_ARRAY_H

#include "memory.h"
#include "assert.h"

namespace hplang
{

// Array for POD-types
template <class T>
struct Array
{
    s64 capacity;
    s64 count;
    T *data;
};

namespace array
{
    template <class T>
    bool Reserve(Array<T> &arr, s64 count);

    template <class T>
    bool Push(Array<T> &arr, const T &x);

    template <class T>
    T At(Array<T> &arr, s64 index);

    template <class T>
    void Free(Array<T> &arr);
}

namespace array
{
    template <class T>
    bool Reserve(Array<T> &arr, s64 count)
    {
        if (arr.capacity >= count) return true;

        s64 old_capacity = arr.capacity;
        s64 new_capacity = count;

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
    T At(Array<T> &arr, s64 index)
    {
        ASSERT(index < arr.count);
        return arr.data[index];
    }

    template <class T>
    void Free(Array<T> &arr)
    {
        Pointer data_p;
        data_p.ptr = arr.data;
        data_p.size = arr.capacity * sizeof(T);
        Free(data_p);
    }
}

} // hplang

#define H_HPLANG_ARRAY_H
#endif
