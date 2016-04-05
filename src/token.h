#ifndef H_HPLANG_TOKEN_H

#include "types.h"
#include "memory.h"

namespace hplang
{

enum TokenType
{
    TOK_Comment,
    TOK_Multiline_comment,

    TOK_StringLit,      // "xyz"
    TOK_CharLit,        // 'x'

    TOK_Identifier,     // identifier

    TOK_Import,         // import
    TOK_If,             // if
    TOK_Else,           // else
    TOK_For,            // for
    TOK_While,          // while
    TOK_Return,         // return
    TOK_Struct,         // struct

    // TODO: Implement enums

    TOK_Type_Bool,      // bool
    TOK_Type_Char,      // char
    TOK_Type_S8,        // s8
    TOK_Type_U8,        // u8
    TOK_Type_S16,       // s16
    TOK_Type_U16,       // u16
    TOK_Type_S32,       // s32
    TOK_Type_U32,       // u32
    TOK_Type_S64,       // s64
    TOK_Type_U64,       // u64
    TOK_Type_String,    // string

    TOK_Hash,           // #
    TOK_DefineConst,    // ::
    TOK_Define,         // :
    TOK_DefineAssign,   // :=
    TOK_Semicolon,      // ;
    TOK_Comma,          // ,
    TOK_Period,         // .
    TOK_OpenBlock,      // {
    TOK_CloseBlock,     // }
    TOK_OpenParent,     // (
    TOK_CloseParent,    // )
    TOK_OpenBracket,    // [
    TOK_CloseBracket,   // ]

    TOK_Eq,             // ==
    TOK_NotEq,          // !=
    TOK_Less,           // <
    TOK_LessEq,         // <=
    TOK_Greater,        // >
    TOK_GreaterEq,      // >=

    TOK_Plus,           // +
    TOK_Minus,          // -
    TOK_Star,           // *
    TOK_Slash,          // /

    TOK_Assign,         // =
    TOK_PlusAssign,     // +=
    TOK_MinusAssign,    // -=
    TOK_StarAssign,     // *=
    TOK_SlashAssign,    // /=

    TOK_And,            // &&
    TOK_Or,             // ||

    // TODO: Add increment & decrement ops (++, --) ??
    // TODO: Add modulo op
    // TODO: Add bitwise ops
    // TODO: Add shift ops
    // TODO: Add ternary op "?"

    TOK_StarStar,       // **

    TOK_COUNT
};

struct Token
{
    const char *value;
    const char *value_end;
    File_Location file_loc;
};

enum {
    DEFAULT_TOKEN_ARENA_SIZE = 4*4096
};

struct Token_Arena
{
    Pointer memory;
    Token *begin;
    const Token *end;
    Token *current;

    Token_Arena *prev_arena;
};

Token_Arena* AllocateTokenArena(Token_Arena *arena, s64 token_count);
void DeallocateTokenArena(Token_Arena *arena);
Token* PushToken(Token_Arena *arena);

} // hplang

#define H_HPLANG_TOKEN_H
#endif
