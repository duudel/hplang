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
    CP_Assembling,
    CP_Linking
};

struct Compiler_Options
{
    const char *output_filename;
    Codegen_Target target;

    s64 max_error_count;
    s64 max_line_arrow_error_count;
    Compilation_Phase stop_after;

    b32 diagnose_memory;
    b32 debug_ir;
    b32 debug_reg_alloc;
};

Compiler_Options DefaultCompilerOptions();

} // hplang

#define H_HPLANG_COMPILER_OPTIONS_H
#endif
