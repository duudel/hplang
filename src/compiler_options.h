#ifndef H_HPLANG_COMPILER_OPTIONS_H

#include "types.h"

namespace hplang
{

enum Compilation_Phase
{
    PHASE_Lexing,
    PHASE_Parsing,
    PHASE_SemanticCheck,
    PHASE_IrGen,
    PHASE_CodeGen,
    PHASE_Assembling,
    PHASE_Linking
};

struct Compiler_Options
{
    const char *output_filename;
    Codegen_Target target;

    s64 max_error_count;
    s64 max_line_arrow_error_count;
    Compilation_Phase stop_after;

    b32 diagnose_memory;
    b32 debug_ast;
    b32 debug_ir;
    b32 debug_reg_alloc;

    b32 profile_time;
    b32 profile_instr_count;
};

Compiler_Options DefaultCompilerOptions();

} // hplang

#define H_HPLANG_COMPILER_OPTIONS_H
#endif
