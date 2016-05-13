#ifndef H_HPLANG_COMPILER_OPTIONS_H

#include "types.h"

namespace hplang
{

enum Compilation_Phase
{
    CP_Lexing,
    CP_Parsing,
    CP_Checking,
    CP_IrGen,
    CP_CodeGen,
};

struct Compiler_Options
{
    s64 max_error_count;
    s64 max_line_arrow_error_count;
    Compilation_Phase stop_after;

    b32 diagnose_memory;
};

Compiler_Options DefaultCompilerOptions();

} // hplang

#define H_HPLANG_COMPILER_OPTIONS_H
#endif
