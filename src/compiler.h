#ifndef H_HPLANG_COMPILER_H

#include "types.h"
#include "memory.h"
#include "error.h"
#include "compiler_options.h"

//#include "token.h"
#include "ast_types.h"
#include "symbols.h"

namespace hplang
{

enum Compilation_Result
{
    RES_OK,
    RES_FAIL_Lexing,
    RES_FAIL_Parsing,
    RES_FAIL_SemanticCheck,
};

struct Module
{
    Ast ast;
    Name module_name;
    Open_File *module_file;
};

typedef Array<Module*> Module_List;

struct Compiler_Context
{
    Memory_Arena arena;
    Error_Context error_ctx;
    Compiler_Options options;

    Module_List modules;
    Environment env;

    Compilation_Result result;
};

Compiler_Context NewCompilerContext();
void FreeCompilerContext(Compiler_Context *ctx);

Open_File* OpenFile(Compiler_Context *ctx, const char *filename);
Open_File* OpenFile(Compiler_Context *ctx, const char *filename,
        const char *filename_end);
Open_File* OpenFile(Compiler_Context *ctx, String filename);
Open_File* OpenModule(Compiler_Context *ctx,
        Open_File *current_file, String filename, String *filename_out);

b32 Compile(Compiler_Context *ctx, Open_File *file);
b32 CompileModule(Compiler_Context *ctx, Open_File *open_file);

b32 ContinueCompiling(Compiler_Context *ctx);
b32 HasError(Compiler_Context *ctx);

void PrintSourceLineAndArrow(Compiler_Context *ctx, File_Location file_loc);

} // hplang

#define H_HPLANG_COMPILER_H
#endif

