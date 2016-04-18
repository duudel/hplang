#ifndef H_HPLANG_COMPILER_OPTIONS_H

#include "types.h"

namespace hplang
{

struct Compiler_Options
{
    s64 max_error_count;
    s64 max_line_arrow_error_count;
};

Compiler_Options DefaultCompilerOptions();

} // hplang

#define H_HPLANG_COMPILER_OPTIONS_H
#endif
