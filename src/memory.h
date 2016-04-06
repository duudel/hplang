#ifndef H_HPLANG_MEMORY_H

#include "types.h"

namespace hplang
{

struct Pointer
{
    void *ptr;
    s64 size;
};

Pointer Alloc(s64 size);
Pointer Realloc(Pointer ptr, s64 new_size);
void Free(Pointer ptr);


// TODO(henrik): Could make private; move to memory.cpp
struct Memory_Block
{
    Pointer memory;
    s64 top_pointer;
    Memory_Block *prev;
};

struct Memory_Arena
{
    Memory_Block *head;
};

void FreeMemoryArena(Memory_Arena *arena);

void* PushData(Memory_Arena *arena, s64 size, s64 alignment);
Pointer PushDataPointer(Memory_Arena *arena, s64 size, s64 alignment);

String PushString(Memory_Arena *arena, const char *s, s64 size);
String PushString(Memory_Arena *arena, const char *s);

template <class S>
S* PushStruct(Memory_Arena *arena)
{
    return (S*)PushData(arena, sizeof(S), alignof(S));
}

} // hplang

#define H_HPLANG_MEMORY_H
#endif
