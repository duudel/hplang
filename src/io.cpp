
#include "io.h"
#include <cstdio>

namespace hplang
{

    s64 PrintString(IoFile *file, String str)
    {
        return fwrite(str.data, 1, str.size, (FILE*)file);
    }

} // hplang
