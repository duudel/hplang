#ifndef H_HPLANG_LEXER_H

#include "token.h"

namespace hplang
{

struct Error_Context;

enum Lexer_Status
{
    LEX_None,
    LEX_Pending,
    LEX_Done,
    LEX_Error
};

struct Lexer_Context
{
    Token_List *tokens;

    s64 current;
    s64 state;
    b32 carriage_return;
    Lexer_Status status;

    Token current_token;

    File_Location file_loc;
    Error_Context *error_ctx;
};

Lexer_Context NewLexerContext(Token_List *tokens, Error_Context *err_ctx);
void FreeLexerContext(Lexer_Context *ctx);

void Lex(Lexer_Context *ctx, const char *text, s64 text_length);

} // hplang

#define H_HPLANG_LEXER_H
#endif
