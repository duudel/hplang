
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

    {TOK_IntLit,            "integer literal"},
    {TOK_UIntLit,           "unsigned integer literal"},
    {TOK_Float32Lit,        "floating point literal (32bit)"},
    {TOK_Float64Lit,        "floating point literal (64bit)"},
    {TOK_CharLit,           "character literal"},
    {TOK_StringLit,         "string literal"},
    {TOK_TrueLit,           "true"},
    {TOK_FalseLit,          "false"},
    {TOK_NullLit,           "null"},

    {TOK_Identifier,        "identifier"},

    {TOK_Break,             "break"},
    {TOK_Continue,          "continue"},
    {TOK_Else,              "else"},
    {TOK_For,               "for"},
    {TOK_Foreign,           "foreign"},
    {TOK_If,                "if"},
    {TOK_Import,            "import"},
    {TOK_Return,            "return"},
    {TOK_Struct,            "struct"},
    {TOK_Typealias,         "typealias"},
    {TOK_While,             "while"},

    {TOK_Type_Void,         "void"},
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
    {TOK_Type_F32,          "f32"},
    {TOK_Type_F64,          "f64"},
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

    {TOK_Bang,              "!"},
    {TOK_AmpAmp,            "&&"},
    {TOK_PipePipe,          "||"},

    {TOK_Arrow,             "->"},
};

template <class T, s64 N>
s64 count_of_arr(T (&)[N])
{ return N; }

static bool check_token_names()
{
    s64 count = count_of_arr(g_token_type_and_str);
    for (s64 i = 0; i < count; i++)
    {
        Token_Type type = (Token_Type)i;
        ASSERT(g_token_type_and_str[i].type == type);
    }
    return true;
}

static const bool tokens_checked = check_token_names();

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
    array::Free(tokens->array);
}

Token* PushTokenList(Token_List *tokens)
{
    array::Push(tokens->array, Token());
    Token *token = &tokens->array.data[tokens->array.count - 1];
    *token = { };
    return token;
}

} // hplang
