
#include "assert.h"

#include <cstdio>

namespace hplang
{

void break_here()
{
    // NOTE(henrik): Let's stop the execution of the program.
    // We want to break to the debugger, so exit(-1) or something similar
    // will not work. This is the "portable" (read "lazy") way to do it.
    *(int*)0 = 1;
}

void Assert(const char *expr, const char *file, s64 line)
{
    fprintf(stderr, "\n\n!ASSERT FAILURE\n\n%s:%lld:\n\n  %s\n\n", file, line, expr);
    fflush(stderr);
    break_here();
}

void InvalidCodePath(const char *file, s64 line)
{
    fprintf(stderr, "\n\n!INVALID CODE PATH\n\n%s:%lld:\n\n", file, line);
    fflush(stderr);
    break_here();
}

} // hplang
