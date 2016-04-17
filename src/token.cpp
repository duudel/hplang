
#include "token.h"
#include "assert.h"

#include <cstdio>

namespace hplang
{

struct Token_Type_And_String
{
    Token_Type type;
    const char *str;
};

static Token_Type_And_String g_token_type_and_str[] = {
    {TOK_Comment,           "comment"},
    {TOK_Multiline_comment, "multiline comment"},

    {TOK_IntegerLit,        "integer literal"},
    {TOK_Float32Lit,        "floating point literal (32bit)"},
    {TOK_Float64Lit,        "floating point literal (64bit)"},
    {TOK_CharLit,           "character literal"},
    {TOK_StringLit,         "string literal"},
    {TOK_TrueLit,           "true"},
    {TOK_FalseLit,          "false"},

    {TOK_Identifier,        "identifier"},

    {TOK_Import,            "import"},
    {TOK_If,                "if"},
    {TOK_Else,              "else"},
    {TOK_For,               "for"},
    {TOK_While,             "while"},
    {TOK_Return,            "return"},
    {TOK_Struct,            "struct"},
    {TOK_Null,              "null"},

    {TOK_Type_Bool,         "bool"},
    {TOK_Type_Char,         "char"},
    {TOK_Type_S8,           "s8"},
    {TOK_Type_U8,           "u8"},
    {TOK_Type_S16,          "s16"},
    {TOK_Type_U16,          "u16"},
    {TOK_Type_S32,          "s32"},
    {TOK_Type_U32,          "u32"},
    {TOK_Type_S64,          "s64"},
    {TOK_Type_U64,          "u64"},
    {TOK_Type_String,       "string"},

    {TOK_Hash,              "#"},
    {TOK_Colon,             ":"},
    {TOK_ColonColon,        "::"},
    {TOK_ColonEq,           ":="},
    {TOK_Semicolon,         ";"},
    {TOK_Comma,             ","},
    {TOK_Period,            "."},
    {TOK_PeriodPeriod,      ".."},
    {TOK_QuestionMark,      "?"},
    {TOK_OpenBlock,         "{"},
    {TOK_CloseBlock,        "}"},
    {TOK_OpenParent,        "("},
    {TOK_CloseParent,       ")"},
    {TOK_OpenBracket,       "["},
    {TOK_CloseBracket,      "]"},

    {TOK_EqEq,              "=="},
    {TOK_NotEq,             "!="},
    {TOK_Less,              "<"},
    {TOK_LessEq,            "<="},
    {TOK_Greater,           ">"},
    {TOK_GreaterEq,         ">="},

    {TOK_Plus,              "+"},
    {TOK_Minus,             "-"},
    {TOK_Star,              "*"},
    {TOK_Slash,             "/"},
    {TOK_Percent,           "%"},

    {TOK_Eq,                "="},
    {TOK_PlusEq,            "+="},
    {TOK_MinusEq,           "-="},
    {TOK_StarEq,            "*="},
    {TOK_SlashEq,           "/="},
    {TOK_PercentEq,         "%="},

    {TOK_Ampersand,         "&"},
    {TOK_Pipe,              "|"},
    {TOK_Hat,               "^"},
    {TOK_Tilde,             "~"},
    {TOK_At,                "@"},

    {TOK_AmpEq,             "&="},
    {TOK_PipeEq,            "|="},
    {TOK_HatEq,             "^="},
    {TOK_TildeEq,           "~="},

    {TOK_Bang,              "!"},
    {TOK_AmpAmp,            "&&"},
    {TOK_PipePipe,          "||"},

    {TOK_StarStar,          "**"},
};

const char* TokenTypeToString(Token_Type type)
{
    ASSERT(type < TOK_COUNT);
    return g_token_type_and_str[type].str;
}


enum {
    DEFAULT_ARENA_TOKEN_COUNT = 4 * 4096
};

void FreeTokenList(Token_List *tokens)
{
    Free(tokens->memory);
}

b32 GrowTokenList(Token_List *tokens)
{
    s64 new_size = tokens->memory.size + DEFAULT_ARENA_TOKEN_COUNT * sizeof(Token);
    Pointer new_memory = Realloc(tokens->memory, new_size);
    if (!new_memory.ptr) return false;

    tokens->memory = new_memory;
    tokens->begin = (Token*)tokens->memory.ptr;
    tokens->end = tokens->begin;
    return true;
}

Token* PushTokenList(Token_List *tokens)
{
    Token *memory_end = (Token*)((char*)tokens->memory.ptr + tokens->memory.size);
    if (tokens->end >= memory_end)
    {
        if (!GrowTokenList(tokens))
            return nullptr;
    }
    Token *result = tokens->end;
    tokens->end++;
    tokens->count = tokens->end - tokens->begin;
    return result;
}

} // hplang
