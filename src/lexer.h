#ifndef H_HPLANG_LEXER_H

#include "token.h"

namespace hplang
{

struct Error_Context;
struct Compiler_Context;

struct Lexer_Context
{
    Token_List *tokens;

    s64 current;
    s64 state;
    b32 carriage_return;

    Token current_token;

    File_Location file_loc;
    Compiler_Context *comp_ctx;
};

Lexer_Context NewLexerContext(Token_List *tokens,
        Open_File *file, Compiler_Context *comp_ctx);
void FreeLexerContext(Lexer_Context *ctx);

void Lex(Lexer_Context *ctx);

} // hplang

#define H_HPLANG_LEXER_H
#endif
