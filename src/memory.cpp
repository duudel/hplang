
#include "hplang.h"
#include "memory.h"
#include "assert.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace hplang
{

Pointer Alloc(s64 size)
{
    Pointer ptr = { };
    return Realloc(ptr, size);
}

Pointer Realloc(Pointer ptr, s64 new_size)
{
    void *new_ptr = realloc(ptr.ptr, new_size);

    ptr.ptr = new_ptr;
    ptr.size = new_ptr ? new_size : 0;
    return ptr;
}

void Free(Pointer ptr)
{
    ASSERT(ptr.ptr != nullptr);
    free(ptr.ptr);
}

// TODO(henrik): Extend memory arena implementation to somethin presented below.
// This would remove or decrease the unused space left in the previous
// memory blocks.
//
// Data structure layout
//   _____________
//  |Memory_Arena|
//  |------------|
//  | head-----, |
//  |__________|_|
//             |
// Memory_Blocks
//             |
//    _____   _v___   _____
//   |....|  |... |  |.   |
//   |    |  |    |  |    |
//   |next-->|next-->|next--o
// o--prev|<--prev|<--prev|
//   |____|  |____|  |____|
//
//   {is     {these have
//    full}     free space}

struct Memory_Block
{
    Pointer memory;
    s64 top_pointer;
    Memory_Block *prev;
};

void FreeMemoryArena(Memory_Arena *arena)
{
    Memory_Block *block = arena->head;
    while (block)
    {
        Memory_Block *prev = block->prev;

        Pointer block_ptr;
        block_ptr.ptr = (void*)block;
        block_ptr.size = block->memory.size + sizeof(Memory_Block);
        Free(block_ptr);

        block = prev;
    }
    arena->head = nullptr;
}

static s64 Align(s64 x, s64 alignment)
{
    ASSERT(alignment > 0);
    s64 mask = alignment - 1;
    return (x + mask) & ~mask;
}

static void* Align(void* ptr, s64 alignment)
{
    return (void*)Align((s64)ptr, alignment);
}

static s64 PointerDiff(void *p1, void *p2)
{
    return (s64)((char*)p1 - (char*)p2);
}

static b32 AllocateNewMemoryBlock(Memory_Arena *arena, s64 min_size)
{
    s64 memory_block_size = MBytes(16);
    min_size = Align(min_size, KBytes(4));
    memory_block_size = (memory_block_size < min_size) ? min_size : memory_block_size;

    Pointer data = Alloc(sizeof(Memory_Block) + memory_block_size);
    if (!data.ptr) return false;

    Memory_Block *block = (Memory_Block*)data.ptr;
    block->memory.ptr = (void*)(block + 1);
    block->memory.size = memory_block_size;
    block->top_pointer = 0;

    block->prev = arena->head;
    arena->head = block;
    return true;
}


static void* AllocateFromMemoryBlock(Memory_Block *block, s64 size, s64 alignment)
{
    if (!block)
        return nullptr;

    s64 top_offset = PointerDiff(Align(block->memory.ptr, alignment),
                                 block->memory.ptr);

    if (block->top_pointer + size + top_offset > block->memory.size)
        return nullptr;

    block->top_pointer += top_offset;
    char *ptr = (char*)block->memory.ptr + block->top_pointer;
    block->top_pointer += size;
    return (void*)ptr;
}


void* PushData(Memory_Arena *arena, s64 size, s64 alignment)
{
    void *ptr = AllocateFromMemoryBlock(arena->head, size, alignment);
    if (!ptr)
    {
        if (!AllocateNewMemoryBlock(arena, size + alignment))
            return nullptr;
        ptr = AllocateFromMemoryBlock(arena->head, size, alignment);
    }
    return ptr;
}

Pointer PushDataPointer(Memory_Arena *arena, s64 size, s64 alignment)
{
    Pointer result = { };
    result.ptr = PushData(arena, size, alignment);
    result.size = result.ptr ? size : 0;
    return result;
}

void* PushArray(Memory_Arena *arena, s64 count, s64 size, s64 alignment)
{
    return PushData(arena, count * size, alignment);
}

String PushString(Memory_Arena *arena, const char *s, const char *end)
{
    return PushString(arena, s, end - s);
}

String PushString(Memory_Arena *arena, const char *s, s64 size)
{
    String result = { };
    result.data = (char*)PushData(arena, size, 1);
    if (result.data)
    {
        for (s64 i = 0; i < size; i++)
            result.data[i] = s[i];
        result.size = size;
    }
    return result;
}

String PushString(Memory_Arena *arena, const char *s)
{
    return PushString(arena, s, strlen(s));
}

String PushNullTerminatedString(Memory_Arena *arena, const char *s, const char *end)
{
    return PushNullTerminatedString(arena, s, end - s);
}

String PushNullTerminatedString(Memory_Arena *arena, const char *s, s64 size)
{
    String result = { };
    result.data = (char*)PushData(arena, size + 1, 1);
    if (result.data)
    {
        for (s64 i = 0; i < size; i++)
            result.data[i] = s[i];
        result.data[size] = 0;
        result.size = size;
    }
    return result;
}

String PushNullTerminatedString(Memory_Arena *arena, const char *s)
{
    return PushNullTerminatedString(arena, s, strlen(s));
}

Name PushName(Memory_Arena *arena, const char *s, const char *end)
{
    Name result = MakeName(PushString(arena, s, end));
    return result;
}

Name PushName(Memory_Arena *arena, const char *s, s64 size)
{
    return PushName(arena, s, s + size);
}

Name PushName(Memory_Arena *arena, const char *s)
{
    return PushName(arena, s, strlen(s));
}

} // hplang
