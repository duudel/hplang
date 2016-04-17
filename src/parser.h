#ifndef H_HPLANG_PARSER_H

#include "ast_types.h"
#include "token.h"
#include "memory.h"
#include "compiler_options.h"

namespace hplang
{

struct Error_Context;

struct Parser_Context
{
    Memory_Arena temp_arena;
    Ast *ast;

    s64 current_token;
    Token_List *tokens;

    Open_File *open_file;
    Error_Context *error_ctx;
    Compiler_Options *options;
};

Parser_Context NewParserContext(
        Ast *ast,
        Token_List *tokens,
        Open_File *open_file,
        Error_Context *error_ctx,
        Compiler_Options *options);

void UnlinkAst(Parser_Context *ctx);
void FreeParserContext(Parser_Context *ctx);

b32 Parse(Parser_Context *ctx);

} // hplang

#define H_HPLANG_PARSER_H
#endif
