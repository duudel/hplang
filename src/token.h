#ifndef H_HPLANG_TOKEN_H

#include "types.h"
#include "memory.h"
#include "array.h"

namespace hplang
{

enum Token_Type
{
    TOK_Comment,
    TOK_Multiline_comment,

    TOK_IntLit,         // 34123543 (w/o sign)
    TOK_UIntLit,        // 34123543u (w/o sign)
    TOK_Float32Lit,     // 0.031f
    TOK_Float64Lit,     // 43.12d or 3.21
    TOK_CharLit,        // 'x'
    TOK_StringLit,      // "xyz"
    TOK_TrueLit,        // true
    TOK_FalseLit,       // false
    TOK_NullLit,        // null

    TOK_Identifier,     // identifier

    TOK_Else,           // else
    TOK_For,            // for
    TOK_Foreign,        // foreign
    TOK_If,             // if
    TOK_Import,         // import
    TOK_Return,         // return
    TOK_Struct,         // struct
    TOK_Typealias,      // typealias
    TOK_While,          // while

    // TODO: Implement enums

    TOK_Type_Void,      // void
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
    TOK_Type_F32,       // f32
    TOK_Type_F64,       // f64
    TOK_Type_String,    // string

    TOK_Hash,           // #
    TOK_Colon,          // :
    TOK_ColonColon,     // ::
    TOK_ColonEq,        // :=
    TOK_Semicolon,      // ;
    TOK_Comma,          // ,
    TOK_Period,         // .
    TOK_PeriodPeriod,   // ..
    TOK_QuestionMark,   // ?
    TOK_OpenBlock,      // {
    TOK_CloseBlock,     // }
    TOK_OpenParent,     // (
    TOK_CloseParent,    // )
    TOK_OpenBracket,    // [
    TOK_CloseBracket,   // ]

    TOK_EqEq,           // ==
    TOK_NotEq,          // !=
    TOK_Less,           // <
    TOK_LessEq,         // <=
    TOK_Greater,        // >
    TOK_GreaterEq,      // >=

    TOK_Plus,           // +
    TOK_Minus,          // -
    TOK_Star,           // *
    TOK_Slash,          // /
    TOK_Percent,        // %

    TOK_Eq,             // =
    TOK_PlusEq,         // +=
    TOK_MinusEq,        // -=
    TOK_StarEq,         // *=
    TOK_SlashEq,        // /=
    TOK_PercentEq,      // /=

    TOK_Ampersand,      // &
    TOK_Pipe,           // |
    TOK_Hat,            // ^
    TOK_Tilde,          // ~
    TOK_At,             // @

    TOK_AmpEq,          // &=
    TOK_PipeEq,         // |=
    TOK_HatEq,          // ^=

    TOK_Bang,           // !
    TOK_AmpAmp,         // &&
    TOK_PipePipe,       // ||

    // TODO: Add increment & decrement ops (++, --) ??
    // TODO: Add shift ops

    TOK_Arrow,          // ->

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
    Array<Token> array;
};

void FreeTokenList(Token_List *tokens);
Token* PushTokenList(Token_List *tokens);

} // hplang

#define H_HPLANG_TOKEN_H
#endif
