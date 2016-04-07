#ifndef H_HPLANG_ASSERT_H

#include "types.h"

#define ASSERT(x) if (!(x)) Assert(#x, __FILE__, __LINE__);

namespace hplang
{

void Assert(const char *expr, const char *file, s64 line);

} // hplang

#define H_HPLANG_ASSERT_H
#endif
