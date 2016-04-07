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
    Memory_Arena arena;
    Ast_Node *ast_root;
    Token_Arena *tokens;

    Error_Context *error_ctx;

    Compiler_Options *options;
};

Parser_Context NewParserContext(
        Memory_Arena *parser_arena,
        Token_Arena *tokens,
        Error_Context *err_ctx);

void FreeParserContext(Parser_Context *ctx);

//b32 Parse(Parser_Context *parser_ctx)
//{
//    Token *token = GetNextToken(parser_ctx->tokens);
//    while (token)
//    {
//        switch (token->type)
//        {
//        case TOK_Identifier:
//            Parse
//        }
//    }
//}

} // hplang

#define H_HPLANG_PARSER_H
#endif
