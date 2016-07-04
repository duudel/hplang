
#include "hplang.h"
#include "time_profiler.h"
#include "compiler.h"
#include "assert.h"

#include <cinttypes>
#include <cstdio>

#ifdef HP_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace hplang
{

struct Profiling_Event
{
    s64 time;
    const char *name;
    s64 begin_event_index;

    s64 scope_index;
};

static const s64 MAX_PROFILING_EVENTS = 1024;
static Profiling_Event event_store[MAX_PROFILING_EVENTS];
static s64 next_event = 0;


#ifdef HP_WIN
// TODO(henrik): Use perf_freq; now we assume that the default frequency is in
// micro seconds.
s64 perf_freq;
static bool InitHighPerfCounter()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    perf_freq = freq.QuadPart;
    return true;
}
bool perf_init = InitHighPerfCounter();

static s64 CurrentTime()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}
#else
// TODO(henrik): Test CurrentTime() on Unix
static s64 CurrentTime()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif


Timed_Scope::Timed_Scope(const char *name)
    : event_index(next_event++)
{
    ASSERT(event_index < MAX_PROFILING_EVENTS);
    event_store[event_index].time = CurrentTime();
    event_store[event_index].name = name;
    event_store[event_index].begin_event_index = -1; // is a begin
}

Timed_Scope::~Timed_Scope()
{
    s64 end_event_index = next_event++;
    event_store[end_event_index].time = CurrentTime();
    event_store[end_event_index].name = "";
    event_store[end_event_index].begin_event_index = event_index;
}


// TODO(henrik): Implement scoping of events correcly.
struct Profiling_Scope
{
    const char *name;
    s64 total_time;
    s32 sample_count;
    s32 depth; 
};

static const s64 MAX_SCOPES = 80;
static Profiling_Scope scopes[MAX_SCOPES];
static s64 scope_index;

static s64 GetScopeIndex(s64 parent_index, const char *name)
{
    for (s64 i = parent_index + 1; i < scope_index; i++)
    {
        if (strcmp(scopes[i].name, name) == 0)
        {
            return i;
        }
    }
    ASSERT(scope_index < MAX_SCOPES);
    return scope_index++;
}

static s64 parent_index_stack[MAX_SCOPES];

void CollateProfilingData(Compiler_Context *ctx)
{
    s64 event_count = next_event;
    next_event = 0;
    if (!ctx->options.profile_time)
        return;

    printf("Timings\n");

    for (s64 i = 0; i < scope_index; i++)
    {
        scopes[i] = { };
    }
    scope_index = 0;

    s64 parent_index_top = 0;
    s64 parent_index = -1;
    for (s64 i = 0; i < event_count; i++)
    {
        Profiling_Event &event = event_store[i];
        if (event.begin_event_index == -1)
        {
            parent_index = 0;
            s64 index = GetScopeIndex(parent_index, event.name);
            //printf("Got index %d for %s\n", (s32)index, event.name);
            event.scope_index = index;
            scopes[index].name = event.name;
            if (scopes[index].depth == 0)
                scopes[index].depth = parent_index_top;

            parent_index_stack[parent_index_top++] = parent_index;
            parent_index = index;
        }
        else
        {
            Profiling_Event begin_ev = event_store[event.begin_event_index];
            s64 elapsed_time = event.time - begin_ev.time;
            //printf("%s: %" PRId64 "\n", begin_ev.name, elapsed_time);

            s64 index = begin_ev.scope_index;
            scopes[index].total_time += elapsed_time;
            scopes[index].sample_count++;

            parent_index = parent_index_stack[--parent_index_top];
            //printf("Get parent index %d\n", (s32)parent_index);
        }
    }

    for (s64 i = 0; i < scope_index; i++)
    {
        Profiling_Scope scope = scopes[i];
        for (s64 p = 0; p < scope.depth; p++)
            printf("  ");

        printf("%s: %" PRId32 ": %" PRId64 " us\n",
               scope.name, scope.sample_count, scope.total_time);
    }
}

} // hplang
