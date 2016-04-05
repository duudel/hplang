
#include "memory.h"

#include <cstdlib>

namespace hplang
{

    Pointer Alloc(s64 size)
    {
        Pointer ptr = { };
        return Realloc(ptr, size);
    }

    Pointer Realloc(Pointer ptr, s64 new_size)
    {
        void *new_ptr = ::realloc(ptr.ptr, new_size);

        ptr.ptr = new_ptr;
        ptr.size = new_ptr ? new_size : 0;
        return ptr;
    }

    void Free(Pointer ptr)
    {
        ::free(ptr.ptr);
    }

} // hplang
