
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
    {TOK_FloatLit,          "floating point literal"},
    {TOK_StringLit,         "string literal"},
    {TOK_CharLit,           "character literal"},

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
    {TOK_ColonColon,        "::"},
    {TOK_Colon,             ":"},
    {TOK_ColonAssign,       ":="},
    {TOK_Semicolon,         ";"},
    {TOK_Comma,             ","},
    {TOK_Period,            "."},
    {TOK_OpenBlock,         "{"},
    {TOK_CloseBlock,        "}"},
    {TOK_OpenParent,        "("},
    {TOK_CloseParent,       ")"},
    {TOK_OpenBracket,       "["},
    {TOK_CloseBracket,      "]"},

    {TOK_Eq,                "=="},
    {TOK_NotEq,             "!="},
    {TOK_Less,              "<"},
    {TOK_LessEq,            "<="},
    {TOK_Greater,           ">"},
    {TOK_GreaterEq,         ">="},

    {TOK_Plus,              "+"},
    {TOK_Minus,             "-"},
    {TOK_Star,              "*"},
    {TOK_Slash,             "/"},

    {TOK_Assign,            "="},
    {TOK_PlusAssign,        "+="},
    {TOK_MinusAssign,       "-="},
    {TOK_StarAssign,        "*="},
    {TOK_SlashAssign,       "/="},

    {TOK_Ampersand,         "&"},
    {TOK_Pipe,              "|"},
    {TOK_Hat,               "^"},
    {TOK_Tilde,             "~"},

    {TOK_AmpAssign,         "&="},
    {TOK_PipeAssign,        "|="},
    {TOK_HatAssign,         "^="},
    {TOK_TildeAssign,       "~="},

    {TOK_Bang,              "!"},
    {TOK_And,               "&&"},
    {TOK_Or,                "||"},

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

Token_Arena* AllocateTokenArena(Token_Arena *arena)
{
    s64 token_count = DEFAULT_ARENA_TOKEN_COUNT;
    s64 needed_size = sizeof(Token_Arena) + token_count * sizeof(Token);
    Pointer pointer = Alloc(needed_size);
    if (!pointer.ptr)
        return arena;

    Token_Arena *new_arena = (Token_Arena*)pointer.ptr;
    Token *tokens_begin = (Token*)(new_arena + 1);
    new_arena->memory = pointer;
    new_arena->begin = tokens_begin;
    new_arena->end = new_arena->begin + token_count;
    new_arena->current = new_arena->begin;
    new_arena->next_arena = nullptr;
    new_arena->prev_arena = arena;

    return new_arena;
}

void FreeTokenArena(Token_Arena *arena)
{
    arena = RewindTokenArena(arena);
    while (arena)
    {
        Token_Arena *next_arena = arena->next_arena;
        Free(arena->memory);
        arena = next_arena;
    }
}

Token* PushToken(Token_Arena *arena)
{
    if (arena->current >= arena->end)
    {
        arena = AllocateTokenArena(arena);
        if (arena->current >= arena->end)
            return nullptr;
    }
    Token tok = { };
    Token *token = arena->current++;
    *token = tok;
    return token;
}

Token_Arena* RewindTokenArena(Token_Arena *arena)
{
    Token_Arena *it = arena;
    while (it)
    {
        it->it = it->begin; // NOTE(henrik): Set the iterator to the begin
        Token_Arena *prev_arena = it->prev_arena;
        arena = it;
        it = prev_arena;
    }
    return arena;
}

Token* GetNextToken(Token_Arena **arena)
{
    Token_Arena *a = *arena;
    if (a->it >= a->end)
    {
        a = a->next_arena;
        if (!a)
            return nullptr;
    }
}

} // hplang
