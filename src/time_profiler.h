#ifndef H_HPLANG_TIME_PROFILER_H

#include "types.h"

#define PROFILE_SCOPE(name) \
    Timed_Scope ts_(name)

namespace hplang
{

struct Timed_Scope
{
    s64 event_index;
    Timed_Scope(const char *name);
    ~Timed_Scope();
};

struct Compiler_Context;

void CollateProfilingData(Compiler_Context *ctx);

} // hplang

#define H_HPLANG_TIME_PROFILER_H
#endif
