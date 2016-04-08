#ifndef H_HPLANG_TOKEN_H

#include "types.h"
#include "memory.h"

namespace hplang
{

enum Token_Type
{
    TOK_Comment,
    TOK_Multiline_comment,

    TOK_IntegerLit,
    TOK_FloatLit,
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
    TOK_Null,           // null

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
    TOK_ColonColon,     // ::
    TOK_Colon,          // :
    TOK_ColonAssign,    // :=
    TOK_Semicolon,      // ;
    TOK_Comma,          // ,
    TOK_Period,         // .
    TOK_QuestionMark,   // ?
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

    TOK_Ampersand,      // &
    TOK_Pipe,           // |
    TOK_Hat,            // ^
    TOK_Tilde,          // ~

    TOK_AmpAssign,      // &=
    TOK_PipeAssign,     // |=
    TOK_HatAssign,      // ^=
    TOK_TildeAssign,    // ~=

    TOK_Bang,           // !
    TOK_And,            // &&
    TOK_Or,             // ||

    // TODO: Add increment & decrement ops (++, --) ??
    // TODO: Add modulo op
    // TODO: Add bitwise ops
    // TODO: Add shift ops
    // TODO: Add ternary op "?"

    TOK_StarStar,       // **

    TOK_EOF,            // Special token that is used to signal
                        // end of token stream in the parser.

    TOK_COUNT
};

struct Token
{
    Token_Type type;
    const char *value;
    const char *value_end;
    File_Location file_loc;
};

const char* TokenTypeToString(Token_Type type);


struct Token_List
{
    Pointer memory;
    Token *begin;
    Token *end;
    s64 count;
};

void FreeTokenList(Token_List *tokens);
Token* PushTokenList(Token_List *tokens);

} // hplang

#define H_HPLANG_TOKEN_H
#endif
