#ifndef H_HPLANG_COMPILER_H

#include "types.h"
#include "memory.h"
#include "error.h"
#include "compiler_options.h"

namespace hplang
{

struct Open_File
{
    String filename;
    Pointer contents;
};


struct Compiler_Context
{
    Memory_Arena arena;
    Error_Context error_ctx;
    Compiler_Options options;
};

Compiler_Context NewCompilerContext();
void FreeCompilerContext(Compiler_Context *ctx);

Open_File* OpenFile(Compiler_Context *ctx, const char *filename);

b32 Compile(Compiler_Context *ctx, Open_File *file);

} // hplang

#define H_HPLANG_COMPILER_H
#endif

