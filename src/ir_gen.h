#ifndef H_HPLANG_IR_GEN_H

#include "memory.h"
#include "ir_types.h"
#include "io.h"

namespace hplang
{

struct Compiler_Context;

struct Ir_Gen_Context
{
    Memory_Arena arena;
    Ir_Routine_List routines;
    Ir_Comment comment;
    Compiler_Context *comp_ctx;
};

Ir_Gen_Context NewIrGenContext(Compiler_Context *comp_ctx);
void FreeIrGenContext(Ir_Gen_Context *ctx);

b32 GenIr(Ir_Gen_Context *ctx);

void PrintIr(IoFile *file, Ir_Gen_Context *ctx);

} // hplang

#define H_HPLANG_IR_GEN_H
#endif
