#ifndef H_HPLANG_IR_GEN_H

#include "memory.h"
#include "ir_types.h"
#include "io.h"

namespace hplang
{

struct Compiler_Context;
struct Environment;

struct Ir_Gen_Context
{
    Memory_Arena arena;

    Array<Ir_Operand> breakables;
    Array<Ir_Operand> continuables;

    Ir_Routine_List routines;
    Array<Name> foreign_routines;
    Array<Symbol*> global_vars;
    Ir_Comment comment;

    Environment *env;
    Compiler_Context *comp_ctx;
};

Ir_Gen_Context NewIrGenContext(Compiler_Context *comp_ctx);
void FreeIrGenContext(Ir_Gen_Context *ctx);

b32 GenIr(Ir_Gen_Context *ctx);

void PrintIr(IoFile *file, Ir_Gen_Context *ctx);

} // hplang

#define H_HPLANG_IR_GEN_H
#endif
