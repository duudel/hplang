
#include "lexer.h"
#include "error.h"
#include "assert.h"

#include <cstdio> // TODO(henrik): remove direct dependency to stdio

namespace hplang
{

Lexer_Context NewLexerContext(Token_List *tokens, Error_Context *error_ctx)
{
    Lexer_Context ctx = { };
    ctx.tokens = tokens;
    ctx.status = LEX_None;
    ctx.file_loc.line = 1;
    ctx.file_loc.column = 1;
    ctx.error_ctx = error_ctx;
    return ctx;
}

void FreeLexerContext(Lexer_Context *ctx)
{
    ctx->tokens = nullptr;
}

b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
}

b32 is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

b32 is_ident(char c)
{
    return is_alpha(c) || is_digit(c) || c == '_';
}

enum Lexer_State
{
    LS_Default,
    LS_Int,
    LS_FloatP,      // 1.
    LS_Float,       // 1.0
    LS_FloatE1,     // 1.0e
    LS_FloatE_Sign, // 1.0e+
    LS_FloatE,      // 1.0e+5 or 1.0e5
    LS_FloatF,      // 1.0f or 1.0e5f
    LS_FloatD,      // 1.0d or 1.0e5d

    LS_StringLit,
    LS_StringLitEsc,
    LS_StringLitEnd,
    LS_CharLit,
    LS_CharLitEsc,
    LS_CharLitEnd,

    LS_Ident,

    LS_STR_b,
    LS_STR_bo,
    LS_STR_boo,
    LS_STR_bool,
    LS_STR_c,
    LS_STR_ch,
    LS_STR_cha,
    LS_STR_char,
    LS_STR_e,
    LS_STR_el,
    LS_STR_els,
    LS_STR_else,
    LS_STR_f,
    LS_STR_f3,
    LS_STR_f32,
    LS_STR_f6,
    LS_STR_f64,
    LS_STR_fa,
    LS_STR_fal,
    LS_STR_fals,
    LS_STR_false,
    LS_STR_fo,
    LS_STR_for,
    LS_STR_fore,
    LS_STR_forei,
    LS_STR_foreig,
    LS_STR_foreign,
    LS_STR_i,
    LS_STR_if,
    LS_STR_im,
    LS_STR_imp,
    LS_STR_impo,
    LS_STR_impor,
    LS_STR_import,
    LS_STR_n,
    LS_STR_nu,
    LS_STR_nul,
    LS_STR_null,
    LS_STR_r,
    LS_STR_re,
    LS_STR_ret,
    LS_STR_retu,
    LS_STR_retur,
    LS_STR_return,
    LS_STR_s,
    LS_STR_s8,
    LS_STR_s1,
    LS_STR_s16,
    LS_STR_s3,
    LS_STR_s32,
    LS_STR_s6,
    LS_STR_s64,
    LS_STR_st,
    LS_STR_str,
    LS_STR_stri,
    LS_STR_strin,
    LS_STR_string,
    LS_STR_stru,
    LS_STR_struc,
    LS_STR_struct,
    LS_STR_t,
    LS_STR_tr,
    LS_STR_tru,
    LS_STR_true,
    LS_STR_u,
    LS_STR_u8,
    LS_STR_u1,
    LS_STR_u16,
    LS_STR_u3,
    LS_STR_u32,
    LS_STR_u6,
    LS_STR_u64,
    LS_STR_w,
    LS_STR_wh,
    LS_STR_whi,
    LS_STR_whil,
    LS_STR_while,

    LS_Hash,            // #
    LS_Colon,           // :
    LS_ColonColon,      // ::
    LS_ColonEq,         // :=
    LS_Semicolon,       // ;
    LS_Comma,           // ,
    LS_Period,          // .
    LS_PeriodPeriod,    // ..
    LS_QuestionMark,    // ?
    LS_OpenBlock,       // {
    LS_CloseBlock,      // }
    LS_OpenParent,      // (
    LS_CloseParent,     // )
    LS_OpenBracket,     // [
    LS_CloseBracket,    // ]

    LS_Eq,              // =
    LS_EqEq,            // ==
    LS_Bang,            // !
    LS_NotEq,           // !=
    LS_Less,            // <
    LS_LessEq,          // <=
    LS_Greater,         // >
    LS_GreaterEq,       // >=

    LS_Plus,            // +
    LS_Minus,           // -
    LS_Star,            // *
    LS_Slash,           // /

    LS_PlusEq,          // +=
    LS_MinusEq,         // -=
    LS_StarEq,          // *=
    LS_SlashEq,         // /=

    LS_Ampersand,       // &
    LS_AmpAmp,          // &&
    LS_Pipe,            // |
    LS_PipePipe,        // ||
    LS_Hat,             // ^
    LS_Tilde,           // ~
    LS_At,              // @

    LS_AmpEq,           // &=
    LS_PipeEq,          // |=
    LS_HatEq,           // ^=
    LS_TildeEq,         // ~=

    LS_Comment,
    LS_MultilineComment,
    LS_MultilineCommentStar,

    LS_Invalid, // Give error
    LS_Junk,    // Discard

    LS_COUNT
};

struct FSM
{
    Lexer_State state;
    b32 emit;
    b32 done;
    b32 carriage_return;
};

static FSM lex_default(FSM fsm, char c, File_Location *file_loc)
{
    if (c == 0)
    {
        if (fsm.state != LS_Default)
            fsm.emit = true;
        fsm.done = true;
        return fsm;
    }

    switch (fsm.state)
    {
    case LS_Default:
        switch (c)
        {
            case ' ': case '\t': case '\r':
            case '\n': case '\f': case '\v':
                fsm.state = LS_Junk;
                break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                fsm.state = LS_Int;
                break;
            case '"':
                fsm.state = LS_StringLit;
                break;
            case '\'':
                fsm.state = LS_CharLit;
                break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
            case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
            case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
            case 'V': case 'W': case 'X': case 'Y': case 'Z':
                fsm.state = LS_Ident; break;
            case 'a': fsm.state = LS_Ident; break;
            case 'b': fsm.state = LS_STR_b; break;
            case 'c': fsm.state = LS_STR_c; break;
            case 'd': fsm.state = LS_Ident; break;
            case 'e': fsm.state = LS_STR_e; break;
            case 'f': fsm.state = LS_STR_f; break;
            case 'g': fsm.state = LS_Ident; break;
            case 'h': fsm.state = LS_Ident; break;
            case 'i': fsm.state = LS_STR_i; break;
            case 'j': fsm.state = LS_Ident; break;
            case 'k': fsm.state = LS_Ident; break;
            case 'l': fsm.state = LS_Ident; break;
            case 'm': fsm.state = LS_Ident; break;
            case 'n': fsm.state = LS_STR_n; break;
            case 'o': fsm.state = LS_Ident; break;
            case 'p': fsm.state = LS_Ident; break;
            case 'q': fsm.state = LS_Ident; break;
            case 'r': fsm.state = LS_STR_r; break;
            case 's': fsm.state = LS_STR_s; break;
            case 't': fsm.state = LS_STR_t; break;
            case 'u': fsm.state = LS_STR_u; break;
            case 'v': fsm.state = LS_Ident; break;
            case 'w': fsm.state = LS_Ident; break;
            case 'x': fsm.state = LS_Ident; break;
            case 'y': fsm.state = LS_Ident; break;
            case 'z': fsm.state = LS_Ident; break;

            case '#': fsm.state = LS_Hash; break;
            case ':': fsm.state = LS_Colon; break;
            case ';': fsm.state = LS_Semicolon; break;
            case ',': fsm.state = LS_Comma; break;
            case '.': fsm.state = LS_Period; break;
            case '?': fsm.state = LS_QuestionMark; break;
            case '{': fsm.state = LS_OpenBlock; break;
            case '}': fsm.state = LS_CloseBlock; break;
            case '(': fsm.state = LS_OpenParent; break;
            case ')': fsm.state = LS_CloseParent; break;
            case '[': fsm.state = LS_OpenBracket; break;
            case ']': fsm.state = LS_CloseBracket; break;
            case '=': fsm.state = LS_Eq; break;
            case '!': fsm.state = LS_Bang; break;
            case '<': fsm.state = LS_Less; break;
            case '>': fsm.state = LS_Greater; break;
            case '+': fsm.state = LS_Plus; break;
            case '-': fsm.state = LS_Minus; break;
            case '*': fsm.state = LS_Star; break;
            case '/': fsm.state = LS_Slash; break;
            case '&': fsm.state = LS_Ampersand; break;
            case '|': fsm.state = LS_Pipe; break;
            case '^': fsm.state = LS_Hat; break;
            case '~': fsm.state = LS_Tilde; break;
            case '@': fsm.state = LS_At; break;

            default:
                fsm.state = LS_Invalid;
                fsm.emit = false;
        } break;

    case LS_Int:
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                break;
            case '.': fsm.state = LS_FloatP; break;
            default:
                fsm.emit = true;
        } break;

    case LS_FloatP:
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                fsm.state = LS_Float; break;
            default:
                // TODO(henrik): new state of flag for better errors?
                fsm.state = LS_Invalid;
        } break;
    case LS_Float:
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                break;
            case 'e': fsm.state = LS_FloatE1; break;
            case 'f': fsm.state = LS_FloatF; break;
            case 'd': fsm.state = LS_FloatD; break;
            default:
                fsm.emit = true;
        } break;
    case LS_FloatE1:
        switch (c)
        {
            case '+': case '-':
                fsm.state = LS_FloatE_Sign; break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                fsm.state = LS_FloatE; break;
            default:
                fsm.emit = true;
        } break;
    case LS_FloatE_Sign:
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                fsm.state = LS_FloatE; break;
            default:
                // TODO(henrik): new state of flag for better errors?
                fsm.state = LS_Invalid;
        } break;
    case LS_FloatE:
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                break;
            case 'f': fsm.state = LS_FloatF; break;
            case 'd': fsm.state = LS_FloatD; break;
            default:
                fsm.emit = true;
        } break;
    case LS_FloatF:
    case LS_FloatD:
        fsm.emit = true;
        break;

    case LS_StringLit:
        if (c == '\\')
        {
            fsm.state = LS_StringLitEsc;
        }
        else if (c == '"')
        {
            fsm.state = LS_StringLitEnd;
        } break;
    case LS_StringLitEsc:
        fsm.state = LS_StringLit;
        break;
    case LS_StringLitEnd:
        fsm.emit = true;
        break;

    case LS_CharLit:
        if (c == '\\')
        {
            fsm.state = LS_CharLitEsc;
        }
        else if (c == '\'')
        {
            fsm.state = LS_CharLitEnd;
        } break;
    case LS_CharLitEsc:
        fsm.state = LS_CharLit;
        break;
    case LS_CharLitEnd:
        fsm.emit = true;
        break;

    case LS_Ident:
        if (is_ident(c))
            fsm.state = LS_Ident;
        else
            fsm.emit = true;
        break;

    #define KW_END_CASE()\
        if (is_ident(c))\
        {\
            fsm.state = LS_Ident;\
        }\
        else\
        {\
            fsm.emit = true;\
        }\
        break

    #define STR_END_CASE()\
        if (is_ident(c))\
        {\
            fsm.state = LS_Ident;\
        }\
        else\
        {\
            fsm.state = LS_Ident;\
            fsm.emit = true;\
        }\
        break

    case LS_STR_b:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_bo; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_bo:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_boo; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_boo:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_bool; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_bool: KW_END_CASE();
    case LS_STR_c:
        switch (c)
        {
            case 'h':
                fsm.state = LS_STR_ch; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_ch:
        switch (c)
        {
            case 'a':
                fsm.state = LS_STR_cha; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_cha:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_char; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_char: KW_END_CASE();
    case LS_STR_e:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_el; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_el:
        switch (c)
        {
            case 's':
                fsm.state = LS_STR_els; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_els:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_else; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_else: KW_END_CASE();
    case LS_STR_f:
        switch (c)
        {
            case '3':
                fsm.state = LS_STR_f3; break;
            case '6':
                fsm.state = LS_STR_f6; break;
            case 'a':
                fsm.state = LS_STR_fa; break;
            case 'o':
                fsm.state = LS_STR_fo; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_f3:
        switch (c)
        {
            case '2':
                fsm.state = LS_STR_f32; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_f32: KW_END_CASE();
    case LS_STR_f6:
        switch (c)
        {
            case '4':
                fsm.state = LS_STR_f64; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_f64: KW_END_CASE();
    case LS_STR_fa:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_fal; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_fal:
        switch (c)
        {
            case 's':
                fsm.state = LS_STR_fals; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_fals:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_false; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_false: KW_END_CASE();
    case LS_STR_fo:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_for; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_for:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_fore; break;
            default:
                KW_END_CASE();
        } break;
    case LS_STR_fore:
        switch (c)
        {
            case 'i':
                fsm.state = LS_STR_forei; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_forei:
        switch (c)
        {
            case 'g':
                fsm.state = LS_STR_foreig; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_foreig:
        switch (c)
        {
            case 'n':
                fsm.state = LS_STR_foreign; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_foreign: KW_END_CASE();
    case LS_STR_i:
        switch (c)
        {
            case 'f':
                fsm.state = LS_STR_if; break;
            case 'm':
                fsm.state = LS_STR_im; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_if: KW_END_CASE();
    case LS_STR_im:
        switch (c)
        {
            case 'p':
                fsm.state = LS_STR_imp; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_imp:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_impo; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_impo:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_impor; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_impor:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_import; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_import: KW_END_CASE();
    case LS_STR_n:
        switch (c)
        {
            case 'u':
                fsm.state = LS_STR_nu; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_nu:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_nul; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_nul:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_null; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_null: KW_END_CASE();
    case LS_STR_r:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_re; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_re:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_ret; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_ret:
        switch (c)
        {
            case 'u':
                fsm.state = LS_STR_retu; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_retu:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_retur; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_retur:
        switch (c)
        {
            case 'n':
                fsm.state = LS_STR_return; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_return: KW_END_CASE();
    case LS_STR_s:
        switch (c)
        {
            case '8':
                fsm.state = LS_STR_s8; break;
            case '1':
                fsm.state = LS_STR_s1; break;
            case '3':
                fsm.state = LS_STR_s3; break;
            case '6':
                fsm.state = LS_STR_s6; break;
            case 't':
                fsm.state = LS_STR_st; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_s8: KW_END_CASE();
    case LS_STR_s1:
        switch (c)
        {
            case '6':
                fsm.state = LS_STR_s16; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_s16: KW_END_CASE();
    case LS_STR_s3:
        switch (c)
        {
            case '2':
                fsm.state = LS_STR_s32; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_s32: KW_END_CASE();
    case LS_STR_s6:
        switch (c)
        {
            case '4':
                fsm.state = LS_STR_s64; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_s64: KW_END_CASE();
    case LS_STR_st:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_str; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_str:
        switch (c)
        {
            case 'i':
                fsm.state = LS_STR_stri; break;
            case 'u':
                fsm.state = LS_STR_stru; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_stri:
        switch (c)
        {
            case 'n':
                fsm.state = LS_STR_strin; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_strin:
        switch (c)
        {
            case 'g':
                fsm.state = LS_STR_string; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_string: KW_END_CASE();
    case LS_STR_stru:
        switch (c)
        {
            case 'c':
                fsm.state = LS_STR_struc; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_struc:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_struct; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_struct: KW_END_CASE();
    case LS_STR_t:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_tr; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_tr:
        switch (c)
        {
            case 'u':
                fsm.state = LS_STR_tru; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_tru:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_true; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_true: KW_END_CASE();
    case LS_STR_u:
        switch (c)
        {
            case '8':
                fsm.state = LS_STR_u8; break;
            case '1':
                fsm.state = LS_STR_u1; break;
            case '3':
                fsm.state = LS_STR_u3; break;
            case '6':
                fsm.state = LS_STR_u6; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_u8: KW_END_CASE();
    case LS_STR_u1:
        switch (c)
        {
            case '6':
                fsm.state = LS_STR_u16; break;
            default:
                KW_END_CASE();
        } break;
    case LS_STR_u16: KW_END_CASE();
    case LS_STR_u3:
        switch (c)
        {
            case '2':
                fsm.state = LS_STR_u32; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_u32: KW_END_CASE();
    case LS_STR_u6:
        switch (c)
        {
            case '4':
                fsm.state = LS_STR_u64; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_u64: KW_END_CASE();
    case LS_STR_w:
        switch (c)
        {
            case 'h':
                fsm.state = LS_STR_wh; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_wh:
        switch (c)
        {
            case 'i':
                fsm.state = LS_STR_whi; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_whi:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_whil; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_whil:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_while; break;
            default:
                STR_END_CASE();
        } break;
    case LS_STR_while: KW_END_CASE();

    case LS_Hash:
    case LS_ColonColon:
    case LS_ColonEq:
    case LS_PeriodPeriod:
    case LS_Semicolon:
    case LS_Comma:
    case LS_QuestionMark:
    case LS_OpenBlock:
    case LS_CloseBlock:
    case LS_OpenParent:
    case LS_CloseParent:
    case LS_OpenBracket:
    case LS_CloseBracket:
        fsm.emit = true;
        break;
    case LS_At:
        fsm.emit = true;
        break;

    case LS_Colon:
        switch (c)
        {
            case ':': fsm.state = LS_ColonColon; break;
            case '=': fsm.state = LS_ColonEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Period:
        switch (c)
        {
            case '.': fsm.state = LS_PeriodPeriod; break;
            default:
                fsm.emit = true;
        } break;

    case LS_Eq:
        switch (c)
        {
            case '=': fsm.state = LS_EqEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Bang:
        switch (c)
        {
            case '=': fsm.state = LS_NotEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Less:
        switch (c)
        {
            case '=': fsm.state = LS_LessEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Greater:
        switch (c)
        {
            case '=': fsm.state = LS_GreaterEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Plus:
        switch (c)
        {
            case '=': fsm.state = LS_PlusEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Minus:
        switch (c)
        {
            case '=': fsm.state = LS_MinusEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Star:
        switch (c)
        {
            case '=': fsm.state = LS_StarEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Slash:
        switch (c)
        {
            case '/': fsm.state = LS_Comment; break;
            case '*': fsm.state = LS_MultilineComment; break;
            case '=': fsm.state = LS_SlashEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Ampersand:
        switch (c)
        {
            case '=': fsm.state = LS_AmpEq; break;
            case '&': fsm.state = LS_AmpAmp; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Pipe:
        switch (c)
        {
            case '=': fsm.state = LS_PipeEq; break;
            case '|': fsm.state = LS_PipePipe; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Hat:
        switch (c)
        {
            case '=': fsm.state = LS_HatEq; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Tilde:
        switch (c)
        {
            case '=': fsm.state = LS_TildeEq; break;
            default:
                fsm.emit = true;
        } break;

    case LS_EqEq:
    case LS_NotEq:
    case LS_LessEq:
    case LS_GreaterEq:
    case LS_PlusEq:
    case LS_MinusEq:
    case LS_StarEq:
    case LS_SlashEq:
    case LS_AmpEq:
    case LS_PipeEq:
    case LS_HatEq:
    case LS_TildeEq:
    case LS_AmpAmp:
    case LS_PipePipe:
        fsm.emit = true;
        break;

    case LS_Comment:
        if (c == '\r' || c == '\n' || c == '\f' || c == '\v')
        {
            fsm.state = LS_Junk;
        } break;
    case LS_MultilineComment:
        if (c == '*')
        {
            fsm.state = LS_MultilineCommentStar;
        } break;
    case LS_MultilineCommentStar:
        if (c == '*')
        { }
        else if (c == '/')
        {
            fsm.state = LS_Junk;
        }
        else
        {
            fsm.state = LS_MultilineComment;
        } break;

        // NOTE(henrik): We don't want to use default here, as we would miss
        // warnings about unhandled enums.
    //default:

    case LS_Junk:
    case LS_Invalid:
    case LS_COUNT:
        INVALID_CODE_PATH;
    }
    //fsm.state = LS_Invalid;
    //fsm.emit = false;
    return fsm;
}

void EmitToken(Lexer_Context *ctx, Lexer_State state)
{
    switch (state)
    {
        case LS_Default:
            INVALID_CODE_PATH;

        case LS_Int:
            ctx->current_token.type = TOK_IntegerLit; break;
        case LS_Float:
            ctx->current_token.type = TOK_Float64Lit; break;
        case LS_FloatE:
            ctx->current_token.type = TOK_Float64Lit; break;
        case LS_FloatF:
            ctx->current_token.type = TOK_Float32Lit; break;
        case LS_FloatD:
            ctx->current_token.type = TOK_Float64Lit; break;

        case LS_CharLit:
        case LS_CharLitEsc:
            INVALID_CODE_PATH;
        case LS_CharLitEnd:
            ctx->current_token.type = TOK_CharLit; break;

        case LS_StringLit:
        case LS_StringLitEsc:
            INVALID_CODE_PATH;
        case LS_StringLitEnd:
            ctx->current_token.type = TOK_StringLit; break;

        case LS_Ident:
            ctx->current_token.type = TOK_Identifier; break;

        case LS_STR_b:
        case LS_STR_bo:
        case LS_STR_boo:
            INVALID_CODE_PATH;
        case LS_STR_bool:
            ctx->current_token.type = TOK_Type_Bool; break;

        case LS_STR_c:
        case LS_STR_ch:
        case LS_STR_cha:
            INVALID_CODE_PATH;
        case LS_STR_char:
            ctx->current_token.type = TOK_Type_Char; break;

        case LS_STR_e:
        case LS_STR_el:
        case LS_STR_els:
            INVALID_CODE_PATH;
        case LS_STR_else:
            ctx->current_token.type = TOK_Else; break;

        case LS_STR_f:
        case LS_STR_f3:
            INVALID_CODE_PATH;
        case LS_STR_f32:
            ctx->current_token.type = TOK_Type_F32; break;

        case LS_STR_f6:
            INVALID_CODE_PATH;
        case LS_STR_f64:
            ctx->current_token.type = TOK_Type_F64; break;

        case LS_STR_fa:
        case LS_STR_fal:
        case LS_STR_fals:
            INVALID_CODE_PATH;
        case LS_STR_false:
            ctx->current_token.type = TOK_FalseLit; break;

        case LS_STR_fo:
            INVALID_CODE_PATH;
        case LS_STR_for:
            ctx->current_token.type = TOK_For; break;

        case LS_STR_fore:
        case LS_STR_forei:
        case LS_STR_foreig:
            INVALID_CODE_PATH;
        case LS_STR_foreign:
            ctx->current_token.type = TOK_Foreign; break;

        case LS_STR_i:
            INVALID_CODE_PATH;
        case LS_STR_if:
            ctx->current_token.type = TOK_If; break;

        case LS_STR_im:
        case LS_STR_imp:
        case LS_STR_impo:
        case LS_STR_impor:
            INVALID_CODE_PATH;
        case LS_STR_import:
            ctx->current_token.type = TOK_Import; break;

        case LS_STR_n:
        case LS_STR_nu:
        case LS_STR_nul:
            INVALID_CODE_PATH;
        case LS_STR_null:
            ctx->current_token.type = TOK_Null; break;

        case LS_STR_r:
        case LS_STR_re:
        case LS_STR_ret:
        case LS_STR_retu:
        case LS_STR_retur:
            INVALID_CODE_PATH;
        case LS_STR_return:
            ctx->current_token.type = TOK_Return; break;

        case LS_STR_s:
        case LS_STR_st:
        case LS_STR_str:
        case LS_STR_stri:
        case LS_STR_strin:
            INVALID_CODE_PATH;
        case LS_STR_string:
            ctx->current_token.type = TOK_Type_String; break;

        case LS_STR_stru:
        case LS_STR_struc:
            INVALID_CODE_PATH;
        case LS_STR_struct:
            ctx->current_token.type = TOK_Struct; break;

        case LS_STR_s8:
            ctx->current_token.type = TOK_Type_S8; break;

        case LS_STR_s1:
            INVALID_CODE_PATH;
        case LS_STR_s16:
            ctx->current_token.type = TOK_Type_S16; break;

        case LS_STR_s3:
            INVALID_CODE_PATH;
        case LS_STR_s32:
            ctx->current_token.type = TOK_Type_S32; break;

        case LS_STR_s6:
            INVALID_CODE_PATH;
        case LS_STR_s64:
            ctx->current_token.type = TOK_Type_S64; break;

        case LS_STR_t:
        case LS_STR_tr:
        case LS_STR_tru:
            INVALID_CODE_PATH;
        case LS_STR_true:
            ctx->current_token.type = TOK_TrueLit; break;

        case LS_STR_u:
            INVALID_CODE_PATH;
        case LS_STR_u8:
            ctx->current_token.type = TOK_Type_U8; break;

        case LS_STR_u1:
            INVALID_CODE_PATH;
        case LS_STR_u16:
            ctx->current_token.type = TOK_Type_U16; break;

        case LS_STR_u3:
            INVALID_CODE_PATH;
        case LS_STR_u32:
            ctx->current_token.type = TOK_Type_U32; break;

        case LS_STR_u6:
            INVALID_CODE_PATH;
        case LS_STR_u64:
            ctx->current_token.type = TOK_Type_U64; break;

        case LS_STR_w:
        case LS_STR_wh:
        case LS_STR_whi:
        case LS_STR_whil:
            INVALID_CODE_PATH;
        case LS_STR_while:
            ctx->current_token.type = TOK_While; break;

        case LS_Hash:
            ctx->current_token.type = TOK_Hash; break;
        case LS_Colon:
            ctx->current_token.type = TOK_Colon; break;
        case LS_ColonColon:
            ctx->current_token.type = TOK_ColonColon; break;
        case LS_ColonEq:
            ctx->current_token.type = TOK_ColonEq; break;
        case LS_Semicolon:
            ctx->current_token.type = TOK_Semicolon; break;
        case LS_Comma:
            ctx->current_token.type = TOK_Comma; break;
        case LS_Period:
            ctx->current_token.type = TOK_Period; break;
        case LS_PeriodPeriod:
            ctx->current_token.type = TOK_PeriodPeriod; break;
        case LS_QuestionMark:
            ctx->current_token.type = TOK_QuestionMark; break;
        case LS_OpenBlock:
            ctx->current_token.type = TOK_OpenBlock; break;
        case LS_CloseBlock:
            ctx->current_token.type = TOK_CloseBlock; break;
        case LS_OpenParent:
            ctx->current_token.type = TOK_OpenParent; break;
        case LS_CloseParent:
            ctx->current_token.type = TOK_CloseParent; break;
        case LS_OpenBracket:
            ctx->current_token.type = TOK_OpenBracket; break;
        case LS_CloseBracket:
            ctx->current_token.type = TOK_CloseBracket; break;

        case LS_Eq:
            ctx->current_token.type = TOK_Eq; break;
        case LS_EqEq:
            ctx->current_token.type = TOK_EqEq; break;
        case LS_Bang:
            ctx->current_token.type = TOK_Bang; break;
        case LS_NotEq:
            ctx->current_token.type = TOK_NotEq; break;
        case LS_Less:
            ctx->current_token.type = TOK_Less; break;
        case LS_LessEq:
            ctx->current_token.type = TOK_LessEq; break;
        case LS_Greater:
            ctx->current_token.type = TOK_Greater; break;
        case LS_GreaterEq:
            ctx->current_token.type = TOK_GreaterEq; break;

        case LS_Plus:
            ctx->current_token.type = TOK_Plus; break;
        case LS_Minus:
            ctx->current_token.type = TOK_Minus; break;
        case LS_Star:
            ctx->current_token.type = TOK_Star; break;
        case LS_Slash:
            ctx->current_token.type = TOK_Slash; break;

        case LS_PlusEq:
            ctx->current_token.type = TOK_PlusEq; break;
        case LS_MinusEq:
            ctx->current_token.type = TOK_MinusEq; break;
        case LS_StarEq:
            ctx->current_token.type = TOK_StarEq; break;
        case LS_SlashEq:
            ctx->current_token.type = TOK_SlashEq; break;

        case LS_Ampersand:
            ctx->current_token.type = TOK_Ampersand; break;
        case LS_AmpAmp:
            ctx->current_token.type = TOK_AmpAmp; break;
        case LS_Pipe:
            ctx->current_token.type = TOK_Pipe; break;
        case LS_PipePipe:
            ctx->current_token.type = TOK_PipePipe; break;
        case LS_Hat:
            ctx->current_token.type = TOK_Hat; break;
        case LS_Tilde:
            ctx->current_token.type = TOK_Tilde; break;
        case LS_At:
            ctx->current_token.type = TOK_At; break;

        case LS_AmpEq:
            ctx->current_token.type = TOK_AmpEq; break;
        case LS_PipeEq:
            ctx->current_token.type = TOK_PipeEq; break;
        case LS_HatEq:
            ctx->current_token.type = TOK_HatEq; break;
        case LS_TildeEq:
            ctx->current_token.type = TOK_TildeEq; break;

        case LS_Comment:
        case LS_MultilineComment:
        case LS_MultilineCommentStar:

        case LS_Invalid:
        case LS_Junk:
        default:
            INVALID_CODE_PATH;
    }
    Token *token = PushTokenList(ctx->tokens);
    *token = ctx->current_token;
}

void Error(Error_Context *ctx, File_Location file_loc,
            const char *message, const Token *token)
{
    AddError(ctx, file_loc);
    if (!token)
    {
        PrintFileLocation(ctx->file, file_loc);
        fprintf(ctx->file, "%s\n", message);
    }
    else
    {
        PrintFileLocation(ctx->file, file_loc);
        fprintf(ctx->file, "%s '", message);
        PrintTokenValue(ctx->file, token);
        fprintf(ctx->file, "'\n");
    }
}

void Lex(Lexer_Context *ctx, const char *text, s64 text_length)
{
    s64 cur = ctx->current;
    ctx->current_token.value = text;
    ctx->current_token.value_end = text;
    ctx->current_token.file_loc = ctx->file_loc;

    FSM fsm = { };
    fsm.emit = false;
    fsm.state = LS_Default;
    fsm.carriage_return = false;

    File_Location *file_loc = &ctx->file_loc;
    while (cur < text_length - 1)
    {
        while (!fsm.emit && cur < text_length)
        {
            char c = text[cur];
            fsm = lex_default(fsm, c, file_loc);

            // NOTE(henrik): There is lexically ambiguouss case where .
            // follows after n digits that could result in either a floating
            // point literal, e.g. 1.5, or in a range 1..15. Thus we need to
            // peek the following character to disambiguate these.
            if (fsm.state == LS_FloatP)
            {
                if (cur + 1 < text_length && text[cur + 1] == '.')
                {
                    fsm.state = LS_Int;
                    fsm.emit = true;
                    continue;
                }
            }

            if (!fsm.emit)
            {
                cur++;
                file_loc->column++;
                if (c == '\r')
                {
                    file_loc->line++;
                    file_loc->column = 1;
                    file_loc->line_offset = cur;
                    ctx->carriage_return = true;
                }
                else
                {
                    if (c == '\n' && ctx->carriage_return)
                    {
                        file_loc->column = 1;
                    }
                    else if (c == '\n' || c == '\f' || c == '\v')
                    {
                        file_loc->line++;
                        file_loc->column = 1;
                        file_loc->line_offset = cur;
                    }
                    ctx->carriage_return = false;
                }

                if (fsm.state == LS_Invalid)
                {
                    ctx->current_token.value_end = text + cur;
                    Error(ctx->error_ctx, ctx->current_token.file_loc,
                        "Invalid token", &ctx->current_token);

                    fsm.state = LS_Default;
                    ctx->file_loc.offset_start = cur;
                    ctx->current_token.value = text + cur;
                    ctx->current_token.file_loc = *file_loc;
                }
                else if (fsm.state == LS_Junk)
                {
                    fsm.state = LS_Default;
                    ctx->file_loc.offset_start = cur;
                    ctx->current_token.value = text + cur;
                    ctx->current_token.value_end = text + cur;
                    ctx->current_token.file_loc = *file_loc;
                }
            }
        }
        if (fsm.emit)
        {
            ctx->file_loc.offset_end = cur;
            ctx->current_token.value_end = text + cur;
            //fprintf(stderr, "Token value: ");
            //PrintTokenValue(stderr, &ctx->current_token);
            //fprintf(stderr, "\n");

            EmitToken(ctx, fsm.state);

            fsm.emit = false;
            fsm.state = LS_Default;
            file_loc->offset_start = cur;
            ctx->current_token.value = text + cur;
            ctx->current_token.file_loc = *file_loc;
        }
    }

    ctx->current = cur;
    if (fsm.done)
    {
        ctx->status = LEX_Done;
    }
    else if (ctx->error_ctx->error_count > 0)
    {
        ctx->status = LEX_Error;
    }
    else
    {
        ctx->status = LEX_Pending;
    }
}

} // hplang
