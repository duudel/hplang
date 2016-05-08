#ifndef H_HPLANG_SEMANTIC_CHECK_H

#include "memory.h"
#include "array.h"

namespace hplang
{

struct Ast;
struct Open_File;
struct Compiler_Context;
struct Environment;

struct Ast_Expr;
struct Scope;

struct Pending_Expr
{
    Ast_Expr *expr;
    Scope *scope;
};

struct Sem_Check_Context
{
    Memory_Arena temp_arena;
    Ast *ast;
    Environment *env;

    // Queue for expressions whose typing could not be checked due to types
    // that were not inferred yet.
    Array<Pending_Expr> pending_exprs;
    s64 infer_pending_types;

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
