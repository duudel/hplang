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

} // hplang

#define H_HPLANG_MEMORY_H
#endif
