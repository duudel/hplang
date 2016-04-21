#ifndef H_HPLANG_MEMORY_H

#include "types.h"

namespace hplang
{

Pointer Alloc(s64 size);
Pointer Realloc(Pointer ptr, s64 new_size);
void Free(Pointer ptr);

struct Memory_Block;

struct Memory_Arena
{
    Memory_Block *head;
};

void FreeMemoryArena(Memory_Arena *arena);

void* PushData(Memory_Arena *arena, s64 size, s64 alignment);
Pointer PushDataPointer(Memory_Arena *arena, s64 size, s64 alignment);

String PushString(Memory_Arena *arena, const char *s, const char *end);
String PushString(Memory_Arena *arena, const char *s, s64 size);
String PushString(Memory_Arena *arena, const char *s);
String PushNullTerminatedString(Memory_Arena *arena, const char *s, const char *end);
String PushNullTerminatedString(Memory_Arena *arena, const char *s, s64 size);
String PushNullTerminatedString(Memory_Arena *arena, const char *s);

Name PushName(Memory_Arena *arena, const char *s, const char *end);
Name PushName(Memory_Arena *arena, const char *s, s64 size);
Name PushName(Memory_Arena *arena, const char *s);

template <class S>
S* PushStruct(Memory_Arena *arena)
{
    return (S*)PushData(arena, sizeof(S), alignof(S));
}

} // hplang

#define H_HPLANG_MEMORY_H
#endif
