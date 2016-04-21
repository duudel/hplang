#ifndef H_HPLANG_PARSER_H

#include "types.h"
#include "memory.h"

namespace hplang
{

struct Ast;
struct Token_List;
struct Compiler_Context;

struct Parser_Context
{
    Memory_Arena temp_arena;
    Ast *ast;

    s64 current_token;
    Token_List *tokens;

    Open_File *open_file;
    Compiler_Context *comp_ctx;
};

Parser_Context NewParserContext(
        Ast *ast,
        Token_List *tokens,
        Open_File *open_file,
        Compiler_Context *comp_ctx);

void UnlinkAst(Parser_Context *ctx);
void FreeParserContext(Parser_Context *ctx);

b32 Parse(Parser_Context *ctx);

} // hplang

#define H_HPLANG_PARSER_H
#endif
