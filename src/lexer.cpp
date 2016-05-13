
#include "lexer.h"
#include "compiler.h"
#include "error.h"
#include "assert.h"

#include <cstdio>

namespace hplang
{

Lexer_Context NewLexerContext(Token_List *tokens,
        Open_File *file, Compiler_Context *comp_ctx)
{
    Lexer_Context ctx = { };
    ctx.tokens = tokens;
    ctx.file_loc.file = file;
    ctx.file_loc.line = 1;
    ctx.file_loc.column = 1;
    ctx.comp_ctx = comp_ctx;
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
    LS_UInt,
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
    LS_STR_ty,
    LS_STR_typ,
    LS_STR_type,
    LS_STR_typea,
    LS_STR_typeal,
    LS_STR_typeali,
    LS_STR_typealia,
    LS_STR_typealias,
    LS_STR_u,
    LS_STR_u8,
    LS_STR_u1,
    LS_STR_u16,
    LS_STR_u3,
    LS_STR_u32,
    LS_STR_u6,
    LS_STR_u64,
    LS_STR_v,
    LS_STR_vo,
    LS_STR_voi,
    LS_STR_void,
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

    LS_Arrow,           // ->

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
    Token_Type token_type;
    b32 emit;
    b32 done;
    b32 carriage_return;
};

static FSM lex_default(FSM fsm, char c)
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
            case 'v': fsm.state = LS_STR_v; break;
            case 'w': fsm.state = LS_STR_w; break;
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
            case 'u': fsm.state = LS_UInt; break;
            case '.': fsm.state = LS_FloatP; break;
            default:
                fsm.token_type = TOK_IntLit;
                fsm.emit = true;
        } break;
    case LS_UInt:
        fsm.token_type = TOK_UIntLit;
        fsm.emit = true;
        break;

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
                fsm.token_type = TOK_Float64Lit;
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
                fsm.token_type = TOK_Float64Lit;
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
                fsm.token_type = TOK_Float64Lit;
                fsm.emit = true;
        } break;
    case LS_FloatF:
        fsm.token_type = TOK_Float32Lit;
        fsm.emit = true;
        break;
    case LS_FloatD:
        fsm.token_type = TOK_Float64Lit;
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
        fsm.token_type = TOK_StringLit;
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
        fsm.token_type = TOK_CharLit;
        fsm.emit = true;
        break;

    case LS_Ident:
        if (is_ident(c))
        {
            fsm.state = LS_Ident;
        }
        else
        {
            fsm.token_type = TOK_Identifier;
            fsm.emit = true;
        }
        break;

    #define KW_END_CASE(tt)\
        if (is_ident(c))\
        {\
            fsm.state = LS_Ident;\
        }\
        else\
        {\
            fsm.token_type = tt;\
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
            fsm.token_type = TOK_Identifier;\
            fsm.emit = true;\
        }\
        break

    #define SINGLE_CASE(ls, ch, lsn)\
    case ls:\
        switch (c)\
        {\
            case ch:\
                fsm.state = lsn; break;\
            default:\
                STR_END_CASE();\
        } break

    SINGLE_CASE(LS_STR_b, 'o', LS_STR_bo);
    SINGLE_CASE(LS_STR_bo, 'o', LS_STR_boo);
    SINGLE_CASE(LS_STR_boo, 'l', LS_STR_bool);
    case LS_STR_bool: KW_END_CASE(TOK_Type_Bool);

    SINGLE_CASE(LS_STR_c, 'h', LS_STR_ch);
    SINGLE_CASE(LS_STR_ch, 'a', LS_STR_cha);
    SINGLE_CASE(LS_STR_cha, 'r', LS_STR_char);
    case LS_STR_char: KW_END_CASE(TOK_Type_Char);

    SINGLE_CASE(LS_STR_e, 'l', LS_STR_el);
    SINGLE_CASE(LS_STR_el, 's', LS_STR_els);
    SINGLE_CASE(LS_STR_els, 'e', LS_STR_else);
    case LS_STR_else: KW_END_CASE(TOK_Else);

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
    SINGLE_CASE(LS_STR_f3, '2', LS_STR_f32);
    case LS_STR_f32: KW_END_CASE(TOK_Type_F32);
    SINGLE_CASE(LS_STR_f6, '4', LS_STR_f64);
    case LS_STR_f64: KW_END_CASE(TOK_Type_F64);

    SINGLE_CASE(LS_STR_fa, 'l', LS_STR_fal);
    SINGLE_CASE(LS_STR_fal, 's', LS_STR_fals);
    SINGLE_CASE(LS_STR_fals, 'e', LS_STR_false);
    case LS_STR_false: KW_END_CASE(TOK_FalseLit);

    SINGLE_CASE(LS_STR_fo, 'r', LS_STR_for);
    case LS_STR_for:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_fore; break;
            default:
                KW_END_CASE(TOK_For);
        } break;
    SINGLE_CASE(LS_STR_fore, 'i', LS_STR_forei);
    SINGLE_CASE(LS_STR_forei, 'g', LS_STR_foreig);
    SINGLE_CASE(LS_STR_foreig, 'n', LS_STR_foreign);
    case LS_STR_foreign: KW_END_CASE(TOK_Foreign);

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
    case LS_STR_if: KW_END_CASE(TOK_If);

    SINGLE_CASE(LS_STR_im, 'p', LS_STR_imp);
    SINGLE_CASE(LS_STR_imp, 'o', LS_STR_impo);
    SINGLE_CASE(LS_STR_impo, 'r', LS_STR_impor);
    SINGLE_CASE(LS_STR_impor, 't', LS_STR_import);
    case LS_STR_import: KW_END_CASE(TOK_Import);

    SINGLE_CASE(LS_STR_n, 'u', LS_STR_nu);
    SINGLE_CASE(LS_STR_nu, 'l', LS_STR_nul);
    SINGLE_CASE(LS_STR_nul, 'l', LS_STR_null);
    case LS_STR_null: KW_END_CASE(TOK_NullLit);

    SINGLE_CASE(LS_STR_r, 'e', LS_STR_re);
    SINGLE_CASE(LS_STR_re, 't', LS_STR_ret);
    SINGLE_CASE(LS_STR_ret, 'u', LS_STR_retu);
    SINGLE_CASE(LS_STR_retu, 'r', LS_STR_retur);
    SINGLE_CASE(LS_STR_retur, 'n', LS_STR_return);
    case LS_STR_return: KW_END_CASE(TOK_Return);

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
    case LS_STR_s8: KW_END_CASE(TOK_Type_S8);
    SINGLE_CASE(LS_STR_s1, '6', LS_STR_s16);
    case LS_STR_s16: KW_END_CASE(TOK_Type_S16);
    SINGLE_CASE(LS_STR_s3, '2', LS_STR_s32);
    case LS_STR_s32: KW_END_CASE(TOK_Type_S32);
    SINGLE_CASE(LS_STR_s6, '4', LS_STR_s64);
    case LS_STR_s64: KW_END_CASE(TOK_Type_S64);

    SINGLE_CASE(LS_STR_st, 'r', LS_STR_str);
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
    SINGLE_CASE(LS_STR_stri, 'n', LS_STR_strin);
    SINGLE_CASE(LS_STR_strin, 'g', LS_STR_string);
    case LS_STR_string: KW_END_CASE(TOK_Type_String);

    SINGLE_CASE(LS_STR_stru, 'c', LS_STR_struc);
    SINGLE_CASE(LS_STR_struc, 't', LS_STR_struct);
    case LS_STR_struct: KW_END_CASE(TOK_Struct);

    //SINGLE_CASE(LS_STR_t, 'r', LS_STR_tr);
    case LS_STR_t:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_tr; break;
            case 'y':
                fsm.state = LS_STR_ty; break;
            default:
                STR_END_CASE();
        } break;
    SINGLE_CASE(LS_STR_tr, 'u', LS_STR_tru);
    SINGLE_CASE(LS_STR_tru, 'e', LS_STR_true);
    case LS_STR_true: KW_END_CASE(TOK_TrueLit);

    SINGLE_CASE(LS_STR_ty, 'p', LS_STR_typ);
    SINGLE_CASE(LS_STR_typ, 'e', LS_STR_type);
    SINGLE_CASE(LS_STR_type, 'a', LS_STR_typea);
    SINGLE_CASE(LS_STR_typea, 'l', LS_STR_typeal);
    SINGLE_CASE(LS_STR_typeal, 'i', LS_STR_typeali);
    SINGLE_CASE(LS_STR_typeali, 'a', LS_STR_typealia);
    SINGLE_CASE(LS_STR_typealia, 's', LS_STR_typealias);
    case LS_STR_typealias: KW_END_CASE(TOK_Typealias);

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
    case LS_STR_u8: KW_END_CASE(TOK_Type_U8);
    SINGLE_CASE(LS_STR_u1, '6', LS_STR_u16);
    case LS_STR_u16: KW_END_CASE(TOK_Type_U16);
    SINGLE_CASE(LS_STR_u3, '2', LS_STR_u32);
    case LS_STR_u32: KW_END_CASE(TOK_Type_U32);
    SINGLE_CASE(LS_STR_u6, '4', LS_STR_u64);
    case LS_STR_u64: KW_END_CASE(TOK_Type_U64);

    SINGLE_CASE(LS_STR_v, 'o', LS_STR_vo);
    SINGLE_CASE(LS_STR_vo, 'i', LS_STR_voi);
    SINGLE_CASE(LS_STR_voi, 'd', LS_STR_void);
    case LS_STR_void: KW_END_CASE(TOK_Type_Void);

    SINGLE_CASE(LS_STR_w, 'h', LS_STR_wh);
    SINGLE_CASE(LS_STR_wh, 'i', LS_STR_whi);
    SINGLE_CASE(LS_STR_whi, 'l', LS_STR_whil);
    SINGLE_CASE(LS_STR_whil, 'e', LS_STR_while);
    case LS_STR_while: KW_END_CASE(TOK_While);

    #define EMIT_CASE(ls, tt)\
    case ls:\
        fsm.token_type = tt;\
        fsm.emit = true;\
        break

    EMIT_CASE(LS_Hash, TOK_Hash);

    case LS_Colon:
        switch (c)
        {
            case ':': fsm.state = LS_ColonColon; break;
            case '=': fsm.state = LS_ColonEq; break;
            default:
                fsm.token_type = TOK_Colon;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_ColonColon, TOK_ColonColon);
    EMIT_CASE(LS_ColonEq, TOK_ColonEq);

    EMIT_CASE(LS_Semicolon, TOK_Semicolon);
    EMIT_CASE(LS_Comma, TOK_Comma);

    case LS_Period:
        switch (c)
        {
            case '.': fsm.state = LS_PeriodPeriod; break;
            default:
                fsm.token_type = TOK_Period;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_PeriodPeriod, TOK_PeriodPeriod);

    EMIT_CASE(LS_QuestionMark, TOK_QuestionMark);
    EMIT_CASE(LS_OpenBlock, TOK_OpenBlock);
    EMIT_CASE(LS_CloseBlock, TOK_CloseBlock);
    EMIT_CASE(LS_OpenParent, TOK_OpenParent);
    EMIT_CASE(LS_CloseParent, TOK_CloseParent);
    EMIT_CASE(LS_OpenBracket, TOK_OpenBracket);
    EMIT_CASE(LS_CloseBracket, TOK_CloseBracket);

    case LS_Eq:
        switch (c)
        {
            case '=': fsm.state = LS_EqEq; break;
            default:
                fsm.token_type = TOK_Eq;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_EqEq, TOK_EqEq);

    case LS_Bang:
        switch (c)
        {
            case '=': fsm.state = LS_NotEq; break;
            default:
                fsm.token_type = TOK_Bang;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_NotEq, TOK_NotEq);

    case LS_Less:
        switch (c)
        {
            case '=': fsm.state = LS_LessEq; break;
            default:
                fsm.token_type = TOK_Less;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_LessEq, TOK_LessEq);

    case LS_Greater:
        switch (c)
        {
            case '=': fsm.state = LS_GreaterEq; break;
            default:
                fsm.token_type = TOK_Greater;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_GreaterEq, TOK_GreaterEq);

    case LS_Plus:
        switch (c)
        {
            case '=': fsm.state = LS_PlusEq; break;
            default:
                fsm.token_type = TOK_Plus;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_PlusEq, TOK_PlusEq);

    case LS_Minus:
        switch (c)
        {
            case '=': fsm.state = LS_MinusEq; break;
            case '>': fsm.state = LS_Arrow; break;
            default:
                fsm.token_type = TOK_Minus;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_MinusEq, TOK_MinusEq);
    EMIT_CASE(LS_Arrow, TOK_Arrow);

    case LS_Star:
        switch (c)
        {
            case '=': fsm.state = LS_StarEq; break;
            default:
                fsm.token_type = TOK_Star;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_StarEq, TOK_StarEq);

    case LS_Slash:
        switch (c)
        {
            case '/': fsm.state = LS_Comment; break;
            case '*': fsm.state = LS_MultilineComment; break;
            case '=': fsm.state = LS_SlashEq; break;
            default:
                fsm.token_type = TOK_Slash;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_SlashEq, TOK_SlashEq);

    case LS_Ampersand:
        switch (c)
        {
            case '=': fsm.state = LS_AmpEq; break;
            case '&': fsm.state = LS_AmpAmp; break;
            default:
                fsm.token_type = TOK_Ampersand;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_AmpEq, TOK_AmpEq);
    EMIT_CASE(LS_AmpAmp, TOK_AmpAmp);

    case LS_Pipe:
        switch (c)
        {
            case '=': fsm.state = LS_PipeEq; break;
            case '|': fsm.state = LS_PipePipe; break;
            default:
                fsm.token_type = TOK_Pipe;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_PipeEq, TOK_PipeEq);
    EMIT_CASE(LS_PipePipe, TOK_PipePipe);

    case LS_Hat:
        switch (c)
        {
            case '=': fsm.state = LS_HatEq; break;
            default:
                fsm.token_type = TOK_Hat;
                fsm.emit = true;
        } break;
    EMIT_CASE(LS_HatEq, TOK_HatEq);

    EMIT_CASE(LS_Tilde, TOK_Tilde);
    EMIT_CASE(LS_At, TOK_At);

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
    return fsm;
}

static void Error(Lexer_Context *ctx, const char *message, const Token *token)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    File_Location file_loc = ctx->current_token.file_loc;

    AddError(err_ctx, file_loc);
    if (!token)
    {
        PrintFileLocation(err_ctx->file, file_loc);
        fprintf((FILE*)err_ctx->file, "%s\n", message);
        PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
    }
    else
    {
        PrintFileLocation(err_ctx->file, file_loc);
        fprintf((FILE*)err_ctx->file, "%s '", message);
        PrintTokenValue(err_ctx->file, token);
        fprintf((FILE*)err_ctx->file, "'\n");
    }
}

static b32 CheckEmitState(Lexer_Context *ctx, FSM fsm)
{
    switch (fsm.state)
    {
        case LS_Default:
            INVALID_CODE_PATH;

        case LS_Int:            return true;
        case LS_UInt:           return true;
        case LS_FloatP:
            Error(ctx, "Invalid floating point number", &ctx->current_token);
            return false;

        case LS_Float:          return true;

        case LS_FloatE1:
        case LS_FloatE_Sign:
            Error(ctx, "Invalid floating point exponent", &ctx->current_token);
            return false;

        case LS_FloatE:         return true;
        case LS_FloatF:         return true;
        case LS_FloatD:         return true;

        case LS_StringLit:
        case LS_StringLitEsc:
            Error(ctx, "Unterminated string literal", nullptr);
            return false;

        case LS_StringLitEnd:   return true;

        case LS_CharLit:
        case LS_CharLitEsc:
            Error(ctx, "Unterminated character lilteral", nullptr);
            return false;

        case LS_CharLitEnd:     return true;

        case LS_Ident:          return true;

        case LS_STR_b:          return true;
        case LS_STR_bo:         return true;
        case LS_STR_boo:        return true;
        case LS_STR_bool:       return true;
        case LS_STR_c:          return true;
        case LS_STR_ch:         return true;
        case LS_STR_cha:        return true;
        case LS_STR_char:       return true;
        case LS_STR_e:          return true;
        case LS_STR_el:         return true;
        case LS_STR_els:        return true;
        case LS_STR_else:       return true;
        case LS_STR_f:          return true;
        case LS_STR_f3:         return true;
        case LS_STR_f32:        return true;
        case LS_STR_f6:         return true;
        case LS_STR_f64:        return true;
        case LS_STR_fa:         return true;
        case LS_STR_fal:        return true;
        case LS_STR_fals:       return true;
        case LS_STR_false:      return true;
        case LS_STR_fo:         return true;
        case LS_STR_for:        return true;
        case LS_STR_fore:       return true;
        case LS_STR_forei:      return true;
        case LS_STR_foreig:     return true;
        case LS_STR_foreign:    return true;
        case LS_STR_i:          return true;
        case LS_STR_if:         return true;
        case LS_STR_im:         return true;
        case LS_STR_imp:        return true;
        case LS_STR_impo:       return true;
        case LS_STR_impor:      return true;
        case LS_STR_import:     return true;
        case LS_STR_n:          return true;
        case LS_STR_nu:         return true;
        case LS_STR_nul:        return true;
        case LS_STR_null:       return true;
        case LS_STR_r:          return true;
        case LS_STR_re:         return true;
        case LS_STR_ret:        return true;
        case LS_STR_retu:       return true;
        case LS_STR_retur:      return true;
        case LS_STR_return:     return true;
        case LS_STR_s:          return true;
        case LS_STR_s8:         return true;
        case LS_STR_s1:         return true;
        case LS_STR_s16:        return true;
        case LS_STR_s3:         return true;
        case LS_STR_s32:        return true;
        case LS_STR_s6:         return true;
        case LS_STR_s64:        return true;
        case LS_STR_st:         return true;
        case LS_STR_str:        return true;
        case LS_STR_stri:       return true;
        case LS_STR_strin:      return true;
        case LS_STR_string:     return true;
        case LS_STR_stru:       return true;
        case LS_STR_struc:      return true;
        case LS_STR_struct:     return true;
        case LS_STR_t:          return true;
        case LS_STR_tr:         return true;
        case LS_STR_tru:        return true;
        case LS_STR_true:       return true;
        case LS_STR_ty:         return true;
        case LS_STR_typ:        return true;
        case LS_STR_type:       return true;
        case LS_STR_typea:      return true;
        case LS_STR_typeal:     return true;
        case LS_STR_typeali:    return true;
        case LS_STR_typealia:   return true;
        case LS_STR_typealias:  return true;
        case LS_STR_u:          return true;
        case LS_STR_u8:         return true;
        case LS_STR_u1:         return true;
        case LS_STR_u16:        return true;
        case LS_STR_u3:         return true;
        case LS_STR_u32:        return true;
        case LS_STR_u6:         return true;
        case LS_STR_u64:        return true;
        case LS_STR_v:          return true;
        case LS_STR_vo:         return true;
        case LS_STR_voi:        return true;
        case LS_STR_void:       return true;
        case LS_STR_w:          return true;
        case LS_STR_wh:         return true;
        case LS_STR_whi:        return true;
        case LS_STR_whil:       return true;
        case LS_STR_while:      return true;

        case LS_Hash:           return true;
        case LS_Colon:          return true;
        case LS_ColonColon:     return true;
        case LS_ColonEq:        return true;
        case LS_Semicolon:      return true;
        case LS_Comma:          return true;
        case LS_Period:         return true;
        case LS_PeriodPeriod:   return true;
        case LS_QuestionMark:   return true;
        case LS_OpenBlock:      return true;
        case LS_CloseBlock:     return true;
        case LS_OpenParent:     return true;
        case LS_CloseParent:    return true;
        case LS_OpenBracket:    return true;
        case LS_CloseBracket:   return true;

        case LS_Eq:             return true;
        case LS_EqEq:           return true;
        case LS_Bang:           return true;
        case LS_NotEq:          return true;
        case LS_Less:           return true;
        case LS_LessEq:         return true;
        case LS_Greater:        return true;
        case LS_GreaterEq:      return true;

        case LS_Plus:           return true;
        case LS_Minus:          return true;
        case LS_Star:           return true;
        case LS_Slash:          return true;

        case LS_PlusEq:         return true;
        case LS_MinusEq:        return true;
        case LS_StarEq:         return true;
        case LS_SlashEq:        return true;

        case LS_Ampersand:      return true;
        case LS_AmpAmp:         return true;
        case LS_Pipe:           return true;
        case LS_PipePipe:       return true;
        case LS_Hat:            return true;
        case LS_Tilde:          return true;
        case LS_At:             return true;

        case LS_AmpEq:          return true;
        case LS_PipeEq:         return true;
        case LS_HatEq:          return true;

        case LS_Arrow:          return true;

        case LS_Comment:
            return false;
        case LS_MultilineComment:
        case LS_MultilineCommentStar:
            Error(ctx, "Unterminated multiline comment", nullptr);
            return false;

        case LS_Invalid:
        case LS_Junk:

        case LS_COUNT:
            INVALID_CODE_PATH;
    }
    return false;
}

void EmitToken(Lexer_Context *ctx, FSM fsm)
{
    if (CheckEmitState(ctx, fsm))
    {
        Token *token = PushTokenList(ctx->tokens);
        ctx->current_token.type = fsm.token_type;
        *token = ctx->current_token;
    }
}

void Lex(Lexer_Context *ctx)
{
    const char *text = (const char*)ctx->file_loc.file->contents.ptr;
    s64 text_length = ctx->file_loc.file->contents.size;

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
            fsm = lex_default(fsm, c);

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
                    Error(ctx, "Invalid token", &ctx->current_token);

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
            // NOTE(henrik): Put \0 back; we do not want to include it into
            // our last token.
            if (fsm.done) cur--;

            ctx->file_loc.offset_end = cur;
            ctx->current_token.value_end = text + cur;
            EmitToken(ctx, fsm);

            fsm.emit = false;
            fsm.state = LS_Default;
            file_loc->offset_start = cur;
            ctx->current_token.value = text + cur;
            ctx->current_token.file_loc = *file_loc;
        }
    }

    // A remain from continuing lexer days.. -.-´
    ctx->current = cur;
}

} // hplang
