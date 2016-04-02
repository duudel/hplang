#ifndef H_HPLANG_LEXER_H

#include "token.h"

namespace hplang
{

enum LexerStatus
{
    LEX_None,
    LEX_Pending,
    LEX_Done,
    LEX_Error
};

struct ErrorContext
{
    s64 error_count;
};

void PrintError(FileInfo *file_info, const char *error);
void PrintError(Token *token, const char *error);

struct LexerContext
{
    TokenArena *token_arena;
    s64 token_count;
    Token *tokens;

    s64 current;
    s64 state;

    Token current_token;

    FileInfo file_info;
    ErrorContext error_ctx;

    LexerStatus status;
};

LexerContext NewLexerContext();

void Lex(LexerContext *ctx, const char *text, s64 text_length);

} // hplang

#define H_HPLANG_LEXER_H
#endif
