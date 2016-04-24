#ifndef H_HPLANG_COMMON_H

#include "types.h"

namespace hplang
{

template <class T, s64 N>
s64 array_length(T (&)[N])
{ return N; }

} // hplang

#define H_HPLANG_COMMON_H
#endif
