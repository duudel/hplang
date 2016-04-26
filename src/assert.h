#ifndef H_HPLANG_ASSERT_H

#include "types.h"

#define ASSERT(x) if (!(x)) Assert(#x, __FILE__, __LINE__)
#define INVALID_CODE_PATH InvalidCodePath(__FILE__, __LINE__)
#define NOT_IMPLEMENTED(s) NotImplemented(__FILE__, __LINE__, s)

namespace hplang
{

void Assert(const char *expr, const char *file, s64 line);
void InvalidCodePath(const char *file, s64 line);
void NotImplemented(const char *file, s64 line, const char *s);

} // hplang

#define H_HPLANG_ASSERT_H
#endif
