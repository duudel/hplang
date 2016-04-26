
#include "assert.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cinttypes>

namespace hplang
{

void break_here()
{
#ifdef _WIN32
    // SIGTRAP is only available on POSIX operating systems
    __builtin_trap();
    //exit(0);
#else
    raise(SIGTRAP);
#endif
}

void Assert(const char *expr, const char *file, s64 line)
{
    fprintf(stderr, "\n%s:%" PRId64 ":1:\n  !!!ASSERT FAILURE\n\n  %s\n\n", file, line, expr);
    fflush(stderr);
    break_here();
}

void InvalidCodePath(const char *file, s64 line)
{
    fprintf(stderr, "\n%s:%" PRId64 ":1:\n  !!!INVALID CODE PATH\n\n", file, line);
    fflush(stderr);
    break_here();
}

void NotImplemented(const char *file, s64 line, const char *s)
{
    fprintf(stderr, "\n%s:%" PRId64 ":1:\n  !!!Not implemented:  %s\n\n", file, line, s);
    fflush(stderr);
}

} // hplang
