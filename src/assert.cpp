
#include "assert.h"

#include <cstdio>

namespace hplang
{

void Assert(const char *expr, const char *file, s64 line)
{
    fprintf(stderr, "!ASSERT FAILURE\n\n%s:%d:\n\n  %s\n\n", file, line, expr);
    // NOTE(henrik): Let's stop the execution of the compiler.
    // We want to break to the debugger, so exit(-1) or something will not
    // work. This is the "portable" (read "lazy") way to do it.
    *(int*)0 = 1;
}

} // hplang
