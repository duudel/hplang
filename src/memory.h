#ifndef H_HPLANG_MEMORY_H

#include "types.h"

namespace hplang
{

Pointer Alloc(s64 size);
Pointer Realloc(Pointer ptr, s64 new_size);
void Free(Pointer ptr);

uptr Align(uptr x, uptr alignment);
void* Align(void* ptr, uptr alignment);


struct Memory_Block;

struct Memory_Arena
{
    Memory_Block *head;
};

void FreeMemoryArena(Memory_Arena *arena);
void GetMemoryArenaUsage(Memory_Arena *arena, s64 *used, s64 *unused);

// The function tries to free previously allocated data. It is not an error to
// fail to do so. The function is used when some data, whose size is unknown at
// the allocation time, is needed. If the initially allocated size was too small,
// it might be possible to reallocate the data. Once called with pointer ptr,
// the pointer is not valid any more. Following calls to allocation functions
// may return the same pointer or a new pointer.
void TryToFreeData(Memory_Arena *arena, void *ptr, s64 size);

void* PushData(Memory_Arena *arena, s64 size, s64 alignment);
Pointer PushDataPointer(Memory_Arena *arena, s64 size, s64 alignment);

void* PushArray(Memory_Arena *arena, s64 count, s64 size, s64 alignment);

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

template <class S>
S* PushArray(Memory_Arena *arena, s64 count)
{
    return (S*)PushArray(arena, count, sizeof(S), alignof(S));
}

} // hplang

#define H_HPLANG_MEMORY_H
#endif
