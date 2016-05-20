#ifndef H_HPLANG_HASHTABLE_H

#include "array.h"
#include "types.h"
#include "assert.h"

namespace hplang
{

namespace hashtable
{

    template <class T>
    void Grow(Array<T*> &arr, s64 grow_size = 32);

    template <class T>
    void Put(Array<T*> &arr, Name name, T *value);

    template <class T>
    T* Remove(Array<T*> &arr, Name name);

    template <class T>
    T* Lookup(Array<T*> &arr, Name name);

} // hashtable


namespace hashtable
{

template <class T>
void Grow(Array<T*> &arr, s64 grow_size /*= 32*/)
{
    s64 table_size = arr.count;
    s64 new_table_size = table_size + grow_size;

    Array<T*> temp = { };
    array::Resize(temp, new_table_size);

    for (s64 i = 0; i < table_size; i++)
    {
        T *value = array::At(arr, i);
        Put(temp, value->name, value);
    }

    array::Free(arr);
    arr = temp;
}

template <class T>
void Put(Array<T*> &arr, Name name, T *value)
{
    if (arr.count == 0)
    {
        array::Resize(arr, 31);
    }

    u32 tries = 0;
    while (tries < 2)
    {
        u32 table_size = arr.count;
        u32 index = name.hash % table_size;
        u32 probe_offset = 0;
        T *old_value = array::At(arr, index);
        while (old_value && probe_offset < table_size)
        {
            probe_offset++;
            u32 i = (index + probe_offset) % table_size;
            old_value = array::At(arr, i);
        }
        if (old_value)
        {
            Grow(arr);
        }
        else
        {
            u32 i = (index + probe_offset) % table_size;
            arr.data[i] = value;
            return;
        }
        tries++;
    }
    // NOTE(henrik): The value should have been inserted after
    // growing the table, but apparently that did not happen.
    INVALID_CODE_PATH;
}

template <class T>
T* Remove(Array<T*> &arr, Name name)
{
    u32 table_size = arr.count;
    if (table_size == 0) return nullptr;

    u32 hash = name.hash % table_size;
    u32 probe_offset = 0;
    T *value = arr.data[hash];
    while (value && probe_offset < table_size)
    {
        if (value->name == name)
        {
            arr.data[(hash + probe_offset) % table_size] = nullptr;
            return value;
        }
        probe_offset++;
        value = arr.data[(hash + probe_offset) % table_size];
    }
    return nullptr;
}

template <class T>
T* Lookup(Array<T*> &arr, Name name)
{
    u32 table_size = arr.count;
    if (table_size == 0) return nullptr;

    u32 hash = name.hash % table_size;
    u32 probe_offset = 0;
    T *value = arr.data[hash];
    while (value && probe_offset < table_size)
    {
        if (value->name == name)
            return value;
        probe_offset++;
        value = arr.data[(hash + probe_offset) % table_size];
    }
    return nullptr;
}

} // hashtable

} // hplang

#define H_HPLANG_HASHTABLE_H
#endif
