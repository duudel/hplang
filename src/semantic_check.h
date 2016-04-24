#ifndef H_HPLANG_SEMANTIC_CHECK_H

#include "memory.h"

namespace hplang
{

struct Ast;
struct Open_File;
struct Compiler_Context;
struct Environment;

struct Sem_Check_Context
{
    Memory_Arena temp_arena;
    Ast *ast;
    Environment *env;

    Open_File *open_file;
    Compiler_Context *comp_ctx;
};

Sem_Check_Context NewSemanticCheckContext(
        Ast *ast,
        Open_File *open_file,
        Compiler_Context *comp_ctx);

void UnlinkAst(Sem_Check_Context *ctx);
void FreeSemanticCheckContext(Sem_Check_Context *ctx);

b32 Check(Sem_Check_Context *ctx);

} // hplang

#define H_HPLANG_SEMANTIC_CHECK_H
#endif
