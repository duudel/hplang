
#include "lexer.h"
#include "error.h"
#include "assert.h"

#include <cstdio> // TODO(henrik): remove direct dependency to stdio
#include <cctype>

namespace hplang
{

Lexer_Context NewLexerContext(Error_Context *error_ctx)
{
    //Token_Arena *arena = AllocateTokenArena(nullptr);
    Lexer_Context ctx = { };
    //ctx.token_arena = arena;
    ctx.tokens = { };
    ctx.status = LEX_None;
    ctx.file_loc.line = 1;
    ctx.file_loc.column = 1;
    ctx.error_ctx = error_ctx;
    return ctx;
}

void FreeLexerContext(Lexer_Context *ctx)
{
    //FreeTokenArena(ctx->token_arena);
    FreeTokenList(&ctx->tokens);
}


enum Lexer_State
{
    LS_Default,
    LS_Int,
    LS_Float,
    LS_FloatF,
    LS_FloatD,

    LS_StringLit,
    LS_StringLitEsc,
    LS_StringLitEnd,
    LS_CharLit,
    LS_CharLitEsc,
    LS_CharLitEnd,

    LS_KW_end,
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
    LS_STR_fo,
    LS_STR_for,
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
    LS_STR_st,
    LS_STR_str,
    LS_STR_stri,
    LS_STR_strin,
    LS_STR_string,
    LS_STR_stru,
    LS_STR_struc,
    LS_STR_struct,
    LS_STR_s8,
    LS_STR_s1,
    LS_STR_s16,
    LS_STR_s3,
    LS_STR_s32,
    LS_STR_s6,
    LS_STR_s64,
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
    LS_Semicolon,       // ;
    LS_Comma,           // ,
    LS_Period,          // .
    LS_QuestionMark,    // ?
    LS_OpenBlock,       // {
    LS_CloseBlock,      // }
    LS_OpenParent,      // (
    LS_CloseParent,     // )
    LS_OpenBracket,     // [
    LS_CloseBracket,    // ]

    LS_Eq,              // =
    LS_EqEq,            // ==
    LS_Bang,
    LS_NotEq,           // !=
    LS_Less,            // <
    LS_LessEq,          // <=
    LS_Greater,         // >
    LS_GreaterEq,       // >=

    LS_Plus,            // +
    LS_Minus,           // -
    LS_Star,            // *
    LS_Slash,           // /

    LS_PlusAssign,      // +=
    LS_MinusAssign,     // -=
    LS_StarAssign,      // *=
    LS_SlashAssign,     // /=

    LS_Ampersand,       // &
    LS_AmpAmp,          // &&
    LS_Pipe,            // |
    LS_PipePipe,        // ||
    LS_Hat,             // ^
    LS_Tilde,           // ~

    LS_AmpAssign,       // &=
    LS_PipeAssign,      // |=
    LS_HatAssign,       // ^=
    LS_TildeAssign,     // ~=

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
            case 't': fsm.state = LS_Ident; break;
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
            case '.': fsm.state = LS_Float; break;
            default:
                fsm.emit = true;
        } break;

    case LS_Float:
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

    case LS_KW_end:
    case LS_Ident:
    case LS_STR_bool:
    case LS_STR_char:
    case LS_STR_else:
    case LS_STR_for:
    case LS_STR_if:
    case LS_STR_import:
    case LS_STR_null:
    case LS_STR_return:
    case LS_STR_string:
    case LS_STR_struct:
    case LS_STR_s8:
    case LS_STR_s16:
    case LS_STR_s32:
    case LS_STR_s64:
    case LS_STR_u8:
    case LS_STR_u16:
    case LS_STR_u32:
    case LS_STR_u64:
    case LS_STR_while:
        if (isalpha(c) || isdigit(c) || c == '_')
            fsm.state = LS_Ident;
        else
            fsm.emit = true;
        break;

    case LS_STR_b:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_bo; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_bo:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_boo; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_boo:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_bool; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_c:
        switch (c)
        {
            case 'h':
                fsm.state = LS_STR_ch; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_ch:
        switch (c)
        {
            case 'a':
                fsm.state = LS_STR_cha; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_cha:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_char; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_e:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_el; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_el:
        switch (c)
        {
            case 's':
                fsm.state = LS_STR_els; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_els:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_else; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_f:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_fo; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_fo:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_for; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_i:
        switch (c)
        {
            case 'f':
                fsm.state = LS_STR_if; break;
            case 'm':
                fsm.state = LS_STR_im; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_im:
        switch (c)
        {
            case 'p':
                fsm.state = LS_STR_imp; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_imp:
        switch (c)
        {
            case 'o':
                fsm.state = LS_STR_impo; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_impo:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_impor; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_impor:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_import; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_n:
        switch (c)
        {
            case 'u':
                fsm.state = LS_STR_nu; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_nu:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_nul; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_nul:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_null; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_r:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_re; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_re:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_ret; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_ret:
        switch (c)
        {
            case 'u':
                fsm.state = LS_STR_retu; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_retu:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_retur; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_retur:
        switch (c)
        {
            case 'n':
                fsm.state = LS_STR_return; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_s:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_st; break;
            case '8':
                fsm.state = LS_STR_s8; break;
            case '1':
                fsm.state = LS_STR_s1; break;
            case '3':
                fsm.state = LS_STR_s3; break;
            case '6':
                fsm.state = LS_STR_s6; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_s1:
        switch (c)
        {
            case '6':
                fsm.state = LS_STR_s16; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_s3:
        switch (c)
        {
            case '2':
                fsm.state = LS_STR_s32; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_s6:
        switch (c)
        {
            case '4':
                fsm.state = LS_STR_s64; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_st:
        switch (c)
        {
            case 'r':
                fsm.state = LS_STR_str; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_str:
        switch (c)
        {
            case 'i':
                fsm.state = LS_STR_stri; break;
            case 'u':
                fsm.state = LS_STR_stru; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_stri:
        switch (c)
        {
            case 'n':
                fsm.state = LS_STR_strin; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_strin:
        switch (c)
        {
            case 'g':
                fsm.state = LS_STR_string; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_stru:
        switch (c)
        {
            case 'c':
                fsm.state = LS_STR_struc; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_struc:
        switch (c)
        {
            case 't':
                fsm.state = LS_STR_struct; break;
            default:
                fsm.state = LS_KW_end;
        } break;
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
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_u1:
        switch (c)
        {
            case '6':
                fsm.state = LS_STR_u16; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_u3:
        switch (c)
        {
            case '2':
                fsm.state = LS_STR_u32; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_u6:
        switch (c)
        {
            case '4':
                fsm.state = LS_STR_u64; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_w:
        switch (c)
        {
            case 'h':
                fsm.state = LS_STR_wh; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_wh:
        switch (c)
        {
            case 'i':
                fsm.state = LS_STR_whi; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_whi:
        switch (c)
        {
            case 'l':
                fsm.state = LS_STR_whil; break;
            default:
                fsm.state = LS_KW_end;
        } break;
    case LS_STR_whil:
        switch (c)
        {
            case 'e':
                fsm.state = LS_STR_while; break;
            default:
                fsm.state = LS_KW_end;
        } break;

    case LS_Hash:
    case LS_ColonColon:
    case LS_Semicolon:
    case LS_Comma:
    case LS_Period:
    case LS_QuestionMark:
    case LS_OpenBlock:
    case LS_CloseBlock:
    case LS_OpenParent:
    case LS_CloseParent:
    case LS_OpenBracket:
    case LS_CloseBracket:
        fsm.emit = true;
        break;

    case LS_Colon:
        switch (c)
        {
            case ':': fsm.state = LS_ColonColon; break;
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
            case '=': fsm.state = LS_PlusAssign; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Minus:
        switch (c)
        {
            case '=': fsm.state = LS_MinusAssign; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Star:
        switch (c)
        {
            case '=': fsm.state = LS_StarAssign; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Slash:
        switch (c)
        {
            case '/': fsm.state = LS_Comment; break;
            case '*': fsm.state = LS_MultilineComment; break;
            case '=': fsm.state = LS_SlashAssign; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Ampersand:
        switch (c)
        {
            case '=': fsm.state = LS_AmpAssign; break;
            case '&': fsm.state = LS_AmpAmp; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Pipe:
        switch (c)
        {
            case '=': fsm.state = LS_PipeAssign; break;
            case '|': fsm.state = LS_PipePipe; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Hat:
        switch (c)
        {
            case '=': fsm.state = LS_HatAssign; break;
            default:
                fsm.emit = true;
        } break;
    case LS_Tilde:
        switch (c)
        {
            case '=': fsm.state = LS_TildeAssign; break;
            default:
                fsm.emit = true;
        } break;

    case LS_EqEq:
    case LS_NotEq:
    case LS_LessEq:
    case LS_GreaterEq:
    case LS_PlusAssign:
    case LS_MinusAssign:
    case LS_StarAssign:
    case LS_SlashAssign:
    case LS_AmpAssign:
    case LS_PipeAssign:
    case LS_HatAssign:
    case LS_TildeAssign:
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

    default:
        fsm.state = LS_Invalid;
        fsm.emit = false;

    }
    return fsm;
}

void Error(Error_Context *ctx, File_Location file_loc,
            const char *message, const Token *token)
{
    AddError(ctx, file_loc);
    if (!token)
    {
        PrintFileLocation(ctx->file, file_loc);
        fprintf(ctx->file, ": %s\n", message);
    }
    else
    {
        PrintFileLocation(ctx->file, file_loc);
        fprintf(ctx->file, ": %s '", message);
        PrintTokenValue(ctx->file, token);
        fprintf(ctx->file, "' (%i)\n", token->value[0]);
    }
}

void EmitToken(Lexer_Context *ctx, Lexer_State state)
{
    switch (state)
    {
        case LS_Default:
            ASSERT(0); break;

        case LS_Int:
            ctx->current_token.type = TOK_IntegerLit; break;
        case LS_Float:
            ctx->current_token.type = TOK_FloatLit; break;
        case LS_FloatF:
            ctx->current_token.type = TOK_FloatLit; break;
        case LS_FloatD:
            ctx->current_token.type = TOK_FloatLit; break;

        case LS_StringLit:
        case LS_StringLitEsc:
            ASSERT(0); break;

        case LS_StringLitEnd:
            ctx->current_token.type = TOK_StringLit; break;

        case LS_CharLit:
        case LS_CharLitEsc:
            ASSERT(0); break;

        case LS_CharLitEnd:
            ctx->current_token.type = TOK_CharLit; break;

        case LS_KW_end:
        case LS_Ident:
            ctx->current_token.type = TOK_Identifier; break;

        case LS_STR_b:
        case LS_STR_bo:
        case LS_STR_boo:
            ASSERT(0); break;

        case LS_STR_bool:
            ctx->current_token.type = TOK_Type_Bool; break;

        case LS_STR_c:
        case LS_STR_ch:
        case LS_STR_cha:
            ASSERT(0); break;

        case LS_STR_char:
            ctx->current_token.type = TOK_Type_Char; break;

        case LS_STR_e:
        case LS_STR_el:
        case LS_STR_els:
            ASSERT(0); break;

        case LS_STR_else:
            ctx->current_token.type = TOK_Else; break;

        case LS_STR_f:
        case LS_STR_fo:
            ASSERT(0); break;

        case LS_STR_for:
            ctx->current_token.type = TOK_For; break;

        case LS_STR_i:
            ASSERT(0); break;

        case LS_STR_if:
            ctx->current_token.type = TOK_If; break;

        case LS_STR_im:
        case LS_STR_imp:
        case LS_STR_impo:
        case LS_STR_impor:
            ASSERT(0); break;

        case LS_STR_import:
            ctx->current_token.type = TOK_Import; break;

        case LS_STR_n:
        case LS_STR_nu:
        case LS_STR_nul:
            ASSERT(0); break;

        case LS_STR_null:
            ctx->current_token.type = TOK_Null; break;

        case LS_STR_r:
        case LS_STR_re:
        case LS_STR_ret:
        case LS_STR_retu:
        case LS_STR_retur:
            ASSERT(0); break;

        case LS_STR_return:
            ctx->current_token.type = TOK_Return; break;

        case LS_STR_s:
        case LS_STR_st:
        case LS_STR_str:
        case LS_STR_stri:
        case LS_STR_strin:
            ASSERT(0); break;

        case LS_STR_string:
            ctx->current_token.type = TOK_Type_String; break;

        case LS_STR_stru:
        case LS_STR_struc:
            ASSERT(0); break;

        case LS_STR_struct:
            ctx->current_token.type = TOK_Struct; break;

        case LS_STR_s8:
            ctx->current_token.type = TOK_Type_S8; break;

        case LS_STR_s1:
            ASSERT(0); break;

        case LS_STR_s16:
            ctx->current_token.type = TOK_Type_S16; break;

        case LS_STR_s3:
            ASSERT(0); break;

        case LS_STR_s32:
            ctx->current_token.type = TOK_Type_S32; break;

        case LS_STR_s6:
            ASSERT(0); break;

        case LS_STR_s64:
            ctx->current_token.type = TOK_Type_S64; break;

        case LS_STR_u:
            ASSERT(0); break;

        case LS_STR_u8:
            ctx->current_token.type = TOK_Type_U8; break;

        case LS_STR_u1:
            ASSERT(0); break;

        case LS_STR_u16:
            ctx->current_token.type = TOK_Type_U16; break;

        case LS_STR_u3:
            ASSERT(0); break;

        case LS_STR_u32:
            ctx->current_token.type = TOK_Type_U32; break;

        case LS_STR_u6:
            ASSERT(0); break;

        case LS_STR_u64:
            ctx->current_token.type = TOK_Type_U64; break;

        case LS_STR_w:
        case LS_STR_wh:
        case LS_STR_whi:
        case LS_STR_whil:
            ASSERT(0); break;

        case LS_STR_while:
            ctx->current_token.type = TOK_While; break;

        case LS_Hash:
            ctx->current_token.type = TOK_Hash; break;
        case LS_Colon:
            ctx->current_token.type = TOK_Colon; break;
        case LS_ColonColon:
            ctx->current_token.type = TOK_ColonColon; break;
        case LS_Semicolon:
            ctx->current_token.type = TOK_Semicolon; break;
        case LS_Comma:
            ctx->current_token.type = TOK_Comma; break;
        case LS_Period:
            ctx->current_token.type = TOK_Period; break;
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
            ctx->current_token.type = TOK_Assign; break;
        case LS_EqEq:
            ctx->current_token.type = TOK_Eq; break;
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

        case LS_PlusAssign:
            ctx->current_token.type = TOK_PlusAssign; break;
        case LS_MinusAssign:
            ctx->current_token.type = TOK_MinusAssign; break;
        case LS_StarAssign:
            ctx->current_token.type = TOK_StarAssign; break;
        case LS_SlashAssign:
            ctx->current_token.type = TOK_SlashAssign; break;

        case LS_Ampersand:
            ctx->current_token.type = TOK_Ampersand; break;
        case LS_AmpAmp:
            ctx->current_token.type = TOK_And; break;
        case LS_Pipe:
            ctx->current_token.type = TOK_Pipe; break;
        case LS_PipePipe:
            ctx->current_token.type = TOK_Or; break;
        case LS_Hat:
            ctx->current_token.type = TOK_Hat; break;
        case LS_Tilde:
            ctx->current_token.type = TOK_Tilde; break;

        case LS_AmpAssign:
            ctx->current_token.type = TOK_AmpAssign; break;
        case LS_PipeAssign:
            ctx->current_token.type = TOK_PipeAssign; break;
        case LS_HatAssign:
            ctx->current_token.type = TOK_HatAssign; break;
        case LS_TildeAssign:
            ctx->current_token.type = TOK_TildeAssign; break;

        case LS_Comment:
        case LS_MultilineComment:
        case LS_MultilineCommentStar:

        case LS_Invalid:
        case LS_Junk:
        default:
            ASSERT(0);
    }
    Token *token = PushTokenList(&ctx->tokens);
    *token = ctx->current_token;
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
            fsm = lex_default(fsm, c, &ctx->file_loc);

            if (!fsm.emit && fsm.state != LS_KW_end)
            {
                cur++;
                file_loc->column++;
                if (c == '\r')
                {
                    file_loc->line++;
                    file_loc->column = 1;
                    ctx->carriage_return = true;
                }
                else
                {
                    if (c == '\n')
                    {
                        if (ctx->carriage_return)
                        {
                            file_loc->column = 1;
                        }
                        else
                        {
                            file_loc->line++;
                            file_loc->column = 1;
                        }
                    }
                    else if (c == '\f' || c == '\v')
                    {
                        file_loc->line++;
                        file_loc->column = 0;
                    }
                    ctx->carriage_return = false;
                }

                if (fsm.state == LS_Invalid)
                {
                    ctx->current_token.value_end = text + cur;
                    Error(ctx->error_ctx, ctx->current_token.file_loc,
                        "Invalid token", &ctx->current_token);

                    fsm.state = LS_Default;
                    ctx->current_token.value = text + cur;
                    ctx->current_token.file_loc = ctx->file_loc;
                }
                else if (fsm.state == LS_Junk)
                {
                    fsm.state = LS_Default;
                    ctx->current_token.value = text + cur;
                    ctx->current_token.value_end = text + cur;
                    ctx->current_token.file_loc = ctx->file_loc;
                }
            }
        }
        if (fsm.emit)
        {
            ctx->current_token.value_end = text + cur;
            //fprintf(stderr, "Token value: ");
            //PrintTokenValue(stderr, &ctx->current_token);
            //fprintf(stderr, "\n");

            EmitToken(ctx, fsm.state);

            fsm.emit = false;
            fsm.state = LS_Default;
            ctx->current_token.value = text + cur;
            ctx->current_token.file_loc = ctx->file_loc;
        }
    }

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
