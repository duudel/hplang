
#include "parser.h"
#include "common.h"
#include "ast_types.h"
#include "compiler.h"
#include "assert.h"

#include <cstdlib>
#include <cstdio>
#include <cerrno>

namespace hplang
{

Parser_Context NewParserContext(
        Ast *ast,
        Token_List *tokens,
        Open_File *open_file,
        Compiler_Context *comp_ctx)
{
    Parser_Context ctx = { };
    ctx.ast = ast;
    ctx.tokens = tokens;
    ctx.open_file = open_file;
    ctx.comp_ctx = comp_ctx;
    return ctx;
}

void FreeParserContext(Parser_Context *ctx)
{
    FreeMemoryArena(&ctx->temp_arena);
    ctx->ast = nullptr;
}

// Parsing
// -------

static void Error(Parser_Context *ctx, const Token *token, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf((FILE*)err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, token->file_loc);
}

static void ErrorInvalidToken(Parser_Context *ctx, const Token *token)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf((FILE*)err_ctx->file, "Invalid token ");
    PrintTokenValue(err_ctx->file, token);
    fprintf((FILE*)err_ctx->file, "\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, token->file_loc);
}

static void ErrorExpected(Parser_Context *ctx,
        const Token *token,
        const char *expected_token)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf((FILE*)err_ctx->file, "Expecting %s\n", expected_token);
    PrintSourceLineAndArrow(ctx->comp_ctx, token->file_loc);
}

static void ErrorExpectedAtEnd(Parser_Context *ctx,
        const Token *token,
        const char *expected_token)
{
    File_Location file_loc = token->file_loc;

    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);

    SeekToEnd(&file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf((FILE*)err_ctx->file, "Expecting %s\n", expected_token);
    PrintSourceLineAndArrow(ctx->comp_ctx, file_loc);
}

static void ErrorUnexpectedEOF(Parser_Context *ctx)
{
    const Token *last_token = &ctx->tokens->array.data[ctx->tokens->array.count - 1];
    Error(ctx, last_token, "Unexpected end of file");
}

static void ErrorBinaryExprRHS(Parser_Context *ctx, const Token *token, Binary_Op op)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    const char *op_str = "";
    switch (op)
    {
        case BIN_OP_Add:        op_str = "+"; break;
        case BIN_OP_Subtract:   op_str = "-"; break;
        case BIN_OP_Multiply:   op_str = "*"; break;
        case BIN_OP_Divide:     op_str = "/"; break;
        case BIN_OP_Modulo:     op_str = "%"; break;
        case BIN_OP_LeftShift:  op_str = "<<"; break;
        case BIN_OP_RightShift: op_str = ">>"; break;
        case BIN_OP_BitAnd:     op_str = "&"; break;
        case BIN_OP_BitOr:      op_str = "|"; break;
        case BIN_OP_BitXor:     op_str = "^"; break;

        case BIN_OP_And:        op_str = "&&"; break;
        case BIN_OP_Or:         op_str = "||"; break;

        case BIN_OP_Equal:      op_str = "=="; break;
        case BIN_OP_NotEqual:   op_str = "!="; break;
        case BIN_OP_Less:       op_str = "<"; break;
        case BIN_OP_LessEq:     op_str = "<="; break;
        case BIN_OP_Greater:    op_str = ">"; break;
        case BIN_OP_GreaterEq:  op_str = ">="; break;

        case BIN_OP_Range:      op_str = ".."; break;
    }
    fprintf((FILE*)err_ctx->file, "Expecting right hand side operand for operator %s\n", op_str);
    PrintSourceLineAndArrow(ctx->comp_ctx, token->file_loc);
}

static void ErrorAssignmentExprRHS(Parser_Context *ctx, const Token *token, Assignment_Op op)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    const char *op_str = "";
    switch (op)
    {
        case AS_OP_Assign:             op_str = "="; break;
        case AS_OP_AddAssign:          op_str = "+="; break;
        case AS_OP_SubtractAssign:     op_str = "-="; break;
        case AS_OP_MultiplyAssign:     op_str = "*="; break;
        case AS_OP_DivideAssign:       op_str = "/="; break;
        case AS_OP_ModuloAssign:       op_str = "%%="; break;
        case AS_OP_LeftShiftAssign:    op_str = "<<="; break;
        case AS_OP_RightShiftAssign:   op_str = ">>="; break;
        case AS_OP_BitAndAssign:       op_str = "&="; break;
        case AS_OP_BitOrAssign:        op_str = "|="; break;
        case AS_OP_BitXorAssign:       op_str = "^="; break;
    }
    fprintf((FILE*)err_ctx->file, "Expecting right hand side operand for operator %s\n", op_str);
    PrintSourceLineAndArrow(ctx->comp_ctx, token->file_loc);
}


static Token eof_token = {
    TOK_EOF,    // token type
    nullptr,    // value
    nullptr,    // value_end
    { }         // file_loc
};

static const Token* GetCurrentToken(Parser_Context *ctx)
{
    if (ctx->current_token < ctx->tokens->array.count)
        return ctx->tokens->array.data + ctx->current_token;
    return &eof_token;
}

static const Token* GetNextToken(Parser_Context *ctx)
{
    //fprintf(stderr, "  : ");
    //PrintTokenValue(stderr, GetCurrentToken(ctx));
    //fprintf(stderr, "\n");
    if (ctx->current_token < ctx->tokens->array.count)
        ctx->current_token++;
    return GetCurrentToken(ctx);
}

static const Token* PeekNextToken(Parser_Context *ctx)
{
    if (ctx->current_token + 1 < ctx->tokens->array.count)
        return ctx->tokens->array.data + ctx->current_token + 1;
    return &eof_token;
}

static void Error(Parser_Context *ctx, const char *message)
{
    Error(ctx, GetCurrentToken(ctx), message);
}

static const Token* Accept(Parser_Context *ctx, Token_Type token_type)
{
    const Token *token = GetCurrentToken(ctx);
    if (token->type == token_type)
    {
        GetNextToken(ctx);
        return token;
    }
    return nullptr;
}

static const Token* Expect(Parser_Context *ctx, Token_Type token_type)
{
    const Token *token = Accept(ctx, token_type);
    if (token) return token;

    token = GetCurrentToken(ctx);
    if (token->type == TOK_EOF)
        ErrorUnexpectedEOF(ctx);
    else
        ErrorExpected(ctx, token, TokenTypeToString(token_type));
    return nullptr;
}

// TODO(henrik): Rename this function to be more descriptive.
static const Token* ExpectAfterLast(Parser_Context *ctx, Token_Type token_type)
{
    const Token *token = Accept(ctx, token_type);
    if (token) return token;

    token = (ctx->current_token > 0) ?
        ctx->tokens->array.data + (ctx->current_token - 1) : GetCurrentToken(ctx);
    if (token->type == TOK_EOF)
        ErrorUnexpectedEOF(ctx);
    else
        ErrorExpectedAtEnd(ctx, token, TokenTypeToString(token_type));
    return nullptr;
}

static b32 ContinueParsing(Parser_Context *ctx)
{
    if (Accept(ctx, TOK_EOF)) return false;
    return ContinueCompiling(ctx->comp_ctx);
}

//static Ast_Node* PushNode(Parser_Context *ctx,
//        Ast_Node_Type node_type, const Token *token)
//{
//    return PushAstNode(ctx->ast, node_type, token);
//}
//
//static Ast_Expr* PushExpr(Parser_Context *ctx,
//        Ast_Expr_Type expr_type, const Token *token)
//{
//    return PushAstExpr(ctx->ast, expr_type, token);
//}


template <class T>
static Ast_Node* PushNode(Parser_Context *ctx,
        Ast_Node_Type node_type, const Token *token)
{
    return PushAstNode<T>(ctx->ast, node_type, token->file_loc);
}

template <class T>
static Ast_Expr* PushExpr(Parser_Context *ctx,
        Ast_Expr_Type expr_type, const Token *token)
{
    return PushAstExpr<T>(ctx->ast, expr_type, token->file_loc);
}


static Ast_Node* ParseType(Parser_Context *ctx)
{
    Ast_Node *type_node = nullptr;
    const Token *token = GetCurrentToken(ctx);
    switch (token->type)
    {
        case TOK_Bang:
            {
                GetNextToken(ctx);
                type_node = PushNode<Ast_Type_Node>(ctx, AST_Type_Function, token);

                ExpectAfterLast(ctx, TOK_OpenParent);

                Ast_Node *param_type = ParseType(ctx);
                Ast_Param_Type *param_types = nullptr;
                s64 param_count = 0;
                if (param_type)
                {
                    param_types = PushStruct<Ast_Param_Type>(&ctx->ast->arena);
                    param_types->type = param_type;
                    param_types->next = nullptr;

                    param_count++;

                    Ast_Param_Type *param_it = param_types;
                    while (Accept(ctx, TOK_Comma) && ContinueParsing(ctx))
                    {
                        Ast_Node *param_type = ParseType(ctx);
                        if (!param_type)
                        {
                            Error(ctx, "Expecting parameter type after ,");
                            break;
                        }

                        Ast_Param_Type *new_param_type = PushStruct<Ast_Param_Type>(&ctx->ast->arena);
                        new_param_type->type = param_type;
                        new_param_type->next = nullptr;

                        param_it->next = new_param_type;
                        param_it = new_param_type;

                        param_count++;
                    }
                }
                type_node->type_node.function.param_count = param_count;
                type_node->type_node.function.param_types = param_types;
                Expect(ctx, TOK_CloseParent);

                ExpectAfterLast(ctx, TOK_Colon);
                Ast_Node *return_type = ParseType(ctx);
                type_node->type_node.function.return_type = return_type;
            } break;
        case TOK_OpenParent:
            {
                GetNextToken(ctx);
                type_node = ParseType(ctx);
                ExpectAfterLast(ctx, TOK_CloseParent);
            } break;

        case TOK_Type_Void:
        case TOK_Type_Bool:
        case TOK_Type_Char:
        case TOK_Type_String:
        case TOK_Type_S8:
        case TOK_Type_S16:
        case TOK_Type_S32:
        case TOK_Type_S64:
        case TOK_Type_U8:
        case TOK_Type_U16:
        case TOK_Type_U32:
        case TOK_Type_U64:
        case TOK_Type_F32:
        case TOK_Type_F64:
        case TOK_Identifier:
            {
                GetNextToken(ctx);
                type_node = PushNode<Ast_Type_Node>(ctx, AST_Type_Plain, token);
                Name name = PushName(&ctx->ast->arena,
                        token->value, token->value_end);
                type_node->type_node.plain.name = name;
            } break;

        default:
            return nullptr;
    }

    while (ContinueParsing(ctx))
    {
        const Token *token = Accept(ctx, TOK_Star);
        if (token)
        {
            Ast_Node *pointer_node = PushNode<Ast_Type_Node>(ctx, AST_Type_Pointer, token);
            s64 indirection = 1;
            while (Accept(ctx, TOK_Star))
            {
                indirection++;
            }
            pointer_node->type_node.pointer.indirection = indirection;
            pointer_node->type_node.pointer.base_type = type_node;
            type_node = pointer_node;
            continue;
        }
        token = Accept(ctx, TOK_OpenBracket);
        if (token)
        {
            Ast_Node *array_node = PushNode<Ast_Type_Node>(ctx, AST_Type_Array, token);
            ExpectAfterLast(ctx, TOK_CloseBracket);
            s64 arrays = 1;
            while (Accept(ctx, TOK_OpenBracket))
            {
                arrays++;
                ExpectAfterLast(ctx, TOK_CloseBracket);
            }
            array_node->type_node.array.array = arrays;
            array_node->type_node.array.base_type = type_node;
            type_node = array_node;
            continue;
        }
        break;
    }
    return type_node;
}

static s64 ConvertInt(const char *s, const char *end)
{
    s64 result = 0;
    while (s != end)
    {
        char c = *s++;
        result *= 10;
        result += s64(c - '0');
    }
    return result;
}

static s64 ConvertUInt(const char *s, const char *end)
{
    ASSERT(end[-1] == 'u');
    end--; // eat 'u' from the end
    s64 result = 0;
    while (s != end)
    {
        char c = *s++;
        result *= 10;
        result += s64(c - '0');
    }
    return result;
}

static f64 ConvertFloat(Parser_Context *ctx, const Token *token)
{
    // NOTE(henrik): As token->value is not null-terminated, we need to make a
    // null-terminated copy before calling strtod.
    // TODO(henrik): Add a temp arena for this kind of allocations (allocations
    // that can be freed immediately after parsing is done).

    const char *s = token->value;
    const char *end = token->value_end;
    String str = PushNullTerminatedString(&ctx->temp_arena, s, end - s);

    char *tailp = nullptr;
    f64 result = strtod(str.data, &tailp);

    if (errno == ERANGE)
    {
        errno = 0;
        Error(ctx, token, "Floating point literal does not fit in f64");
        return 0.0;
    }
    return result;
}

static char ConvertChar(Parser_Context *ctx, const char *s, const char *end)
{
    ASSERT(s[0] == '\'');
    ASSERT(end[-1] == '\'');

    s++;
    end--;

    char result = 0;
    if (s != end)
    {
        char c = *s++;
        switch (c)
        {
            case '\\':
            {
                if (s != end)
                {
                    c = *s++;
                    switch (c)
                    {
                        case '\\': c = '\\'; break;
                        case '"': c = '"'; break;
                        case '\'': c = '\''; break;
                        case 't': c = '\t'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                    }
                }
                else
                {
                    Error(ctx, "Invalid character escape sequence");
                }
            }
            break;
        }
        result = c;
    }
    // NOTE(henrik): If we did not get to the end of the literal, there is a
    // bug in either the lexer or the conversion above.
    if (s != end)
    {
        INVALID_CODE_PATH;
    }
    return result;
}

static String ConvertString(Parser_Context *ctx, const char *s, const char *end)
{
    // NOTE(henrik): Assumes that the resulting string can only be as long or
    // shorter than the original string. This is due to escaping sequences
    // that never resulting more characters than is used to represent them.
    // We could also use the fact that no one else is going to allocate from
    // the same arena, as we construct the string value, to allocate each
    // character as we advance the string to make the used memory match the
    // length of the string (so no extra memory is reserved).

    ASSERT(s[0] == '"');
    ASSERT(end[-1] == '"');

    s++;
    end--;

    String result = PushString(&ctx->ast->arena, s, end - s);
    s64 i = 0;
    while (s != end)
    {
        char c = *s++;
        switch (c)
        {
            case '\\':
            {
                // TODO(henrik): Implement hex and unicode escape sequences
                if (s != end)
                {
                    c = *s++;
                    switch (c)
                    {
                        case '\\': c = '\\'; break;
                        case '"': c = '"'; break;
                        case '\'': c = '\''; break;
                        case 't': c = '\t'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                    }
                }
                else
                {
                    Error(ctx, "Invalid string escape sequence");
                    continue;
                }
            }
            break;
        }
        result.data[i++] = c;
    }
    result.size = i;
    return result;
}

static Ast_Expr* ParseLiteralExpr(Parser_Context *ctx)
{
    const Token *token = Accept(ctx, TOK_NullLit);
    if (token)
    {
        return PushExpr<Ast_Int_Literal>(ctx, AST_Null, token); // TODO(henrik): needs an empty Ast_Null_Literal struct
    }
    token = Accept(ctx, TOK_TrueLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Bool_Literal>(ctx, AST_BoolLiteral, token);
        literal->bool_literal.value = true;
        return literal;
    }
    token = Accept(ctx, TOK_FalseLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Bool_Literal>(ctx, AST_BoolLiteral, token);
        literal->bool_literal.value = false;
        return literal;
    }
    token = Accept(ctx, TOK_IntLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Int_Literal>(ctx, AST_IntLiteral, token);
        literal->int_literal.value = ConvertInt(token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_UIntLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Int_Literal>(ctx, AST_UIntLiteral, token);
        literal->int_literal.value = ConvertUInt(token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_Float32Lit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Float32_Literal>(ctx, AST_Float32Literal, token);
        literal->float32_literal.value = ConvertFloat(ctx, token);
        return literal;
    }
    token = Accept(ctx, TOK_Float64Lit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Float64_Literal>(ctx, AST_Float64Literal, token);
        literal->float64_literal.value = ConvertFloat(ctx, token);
        return literal;
    }
    token = Accept(ctx, TOK_CharLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_Char_Literal>(ctx, AST_CharLiteral, token);
        literal->char_literal.value = ConvertChar(ctx, token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_StringLit);
    if (token)
    {
        Ast_Expr *literal = PushExpr<Ast_String_Literal>(ctx, AST_StringLiteral, token);
        literal->string_literal.value = ConvertString(ctx, token->value, token->value_end);
        return literal;
    }
    return nullptr;
}

static Ast_Expr* ParsePrefixOperator(Parser_Context *ctx)
{
    const Token *op_token = Accept(ctx, TOK_Plus);
    if (!op_token) op_token = Accept(ctx, TOK_Minus);
    if (!op_token) op_token = Accept(ctx, TOK_Tilde);
    if (!op_token) op_token = Accept(ctx, TOK_Bang);
    if (!op_token) op_token = Accept(ctx, TOK_Ampersand);
    if (!op_token) op_token = Accept(ctx, TOK_At);
    if (!op_token) return nullptr;

    Ast_Expr *pre_op = PushExpr<Ast_Unary_Expr>(ctx, AST_UnaryExpr, op_token);

    Unary_Op op;
    switch (op_token->type)
    {
        case TOK_Plus:      op = UN_OP_Positive; break;
        case TOK_Minus:     op = UN_OP_Negative; break;
        case TOK_Tilde:     op = UN_OP_Complement; break;
        case TOK_Bang:      op = UN_OP_Not; break;
        case TOK_Ampersand: op = UN_OP_Address; break;
        case TOK_At:        op = UN_OP_Deref; break;
        default: INVALID_CODE_PATH;
    }
    pre_op->unary_expr.op = op;
    pre_op->unary_expr.expr = nullptr;
    return pre_op;
}

static Ast_Expr* ParseExpression(Parser_Context *ctx);

static void ParseFunctionArgs(Parser_Context *ctx, Ast_Function_Call *function_call)
{
    Ast_Expr *arg = ParseExpression(ctx);
    if (arg)
    {
        PushExprList(&function_call->args, arg);
        while (Accept(ctx, TOK_Comma) && ContinueParsing(ctx))
        {
            Ast_Expr *arg = ParseExpression(ctx);
            if (!arg)
            {
                Error(ctx, "Expecting function argument after comma");
                break;
            }
            PushExprList(&function_call->args, arg);
        }
    }
}

static Ast_Expr* ParsePostfixOperator(Parser_Context *ctx, Ast_Expr *factor)
{
    const Token *op_token = Accept(ctx, TOK_Period);
    if (op_token)
    {
        Ast_Expr *access_expr = PushExpr<Ast_Access_Expr>(ctx, AST_AccessExpr, op_token);

        const Token *ident_tok = Accept(ctx, TOK_Identifier);
        if (ident_tok)
        {
            Ast_Expr *member_ref  = PushExpr<Ast_Variable_Ref>(ctx, AST_VariableRef, ident_tok);
            Name name = PushName(&ctx->ast->arena,
                    ident_tok->value, ident_tok->value_end);
            member_ref->variable_ref.name = name;

            access_expr->access_expr.base = factor;
            access_expr->access_expr.member = member_ref;
        }
        else
        {
            Error(ctx, op_token, "Expecting identifier");
        }
        return access_expr;
    }
    op_token = Accept(ctx, TOK_OpenBracket);
    if (op_token)
    {
        Ast_Expr *subscript_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_SubscriptExpr, op_token);
        subscript_expr->subscript_expr.base = factor;
        subscript_expr->subscript_expr.index = ParseExpression(ctx);
        if (!subscript_expr->subscript_expr.index)
            Error(ctx, "Expecting subscript expression");
        Expect(ctx, TOK_CloseBracket);
        return subscript_expr;
    }
    op_token = Accept(ctx, TOK_OpenParent);
    if (op_token)
    {
        Ast_Expr *fcall_expr = PushExpr<Ast_Function_Call>(ctx, AST_FunctionCall, op_token);
        fcall_expr->function_call.fexpr = factor;
        ParseFunctionArgs(ctx, &fcall_expr->function_call);
        ExpectAfterLast(ctx, TOK_CloseParent);
        return fcall_expr;
    }
    op_token = Accept(ctx, TOK_Arrow);
    if (op_token)
    {
        Ast_Expr *cast_expr = PushExpr<Ast_Typecast_Expr>(ctx, AST_TypecastExpr, op_token);
        cast_expr->typecast_expr.expr = factor;
        Ast_Node *type = ParseType(ctx);
        if (!type)
        {
            const Token *star_tok = Accept(ctx, TOK_Star);
            if (star_tok)
            {
                Error(ctx, star_tok, "For pointer types * comes after the typename");
                if (ContinueParsing(ctx))
                    type = ParseType(ctx);
            }
            if (!type)
                Error(ctx, op_token, "Expecting type after typecast operator ->");
        }
        cast_expr->typecast_expr.type = type;
        return cast_expr;
    }
    return nullptr;
}

static Ast_Expr* ParseFactorExpr(Parser_Context *ctx)
{
    Ast_Expr *pre_op = ParsePrefixOperator(ctx);

    Ast_Expr *factor = ParseLiteralExpr(ctx);

    if (!factor)
    {
        const Token *ident_tok = Accept(ctx, TOK_Identifier);
        if (ident_tok)
        {
            Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
            factor = PushExpr<Ast_Variable_Ref>(ctx, AST_VariableRef, ident_tok);
            factor->variable_ref.name = name;
        }
    }

    if (!factor)
    {
        const Token *parent_tok = Accept(ctx, TOK_OpenParent);
        if (parent_tok)
        {
            factor = ParseExpression(ctx);
            if (!factor)
            {
                Error(ctx, parent_tok,
                        "Expecting expression after left parenthesis");
                // Eat away the closing parenthesis, but do not generate error.
                Accept(ctx, TOK_CloseParent);
            }
            else
            {
                // We don't want extra errors generated for this problem, so
                // we only expect the closing parenthesis, if the expression
                // was found.
                ExpectAfterLast(ctx, TOK_CloseParent);
            }
        }
    }

    if (!factor)
    {
        if (pre_op)
        {
            Error(ctx, "Expecting operand for unary prefix operator");
        }
        return nullptr;
    }

    // TODO(henrik): post ops
    Ast_Expr *post_op = ParsePostfixOperator(ctx, factor);
    if (post_op)
    {
        factor = post_op;
        while (ContinueParsing(ctx))
        {
            post_op = ParsePostfixOperator(ctx, factor);
            if (!post_op) break;
            factor = post_op;
        }
    }

    if (pre_op)
    {
        pre_op->unary_expr.expr = factor;
        factor = pre_op;
    }

    return factor;
}

static Ast_Expr* ParseMultDivExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseFactorExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Star);
        if (!op_token) op_token = Accept(ctx, TOK_Slash);
        if (!op_token) op_token = Accept(ctx, TOK_Percent);
        if (!op_token) break;

        Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Star:      op = BIN_OP_Multiply; break;
            case TOK_Slash:     op = BIN_OP_Divide; break;
            case TOK_Percent:   op = BIN_OP_Modulo; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = op;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseFactorExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseAddSubExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseMultDivExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Plus);
        if (!op_token) op_token = Accept(ctx, TOK_Minus);
        if (!op_token) op_token = Accept(ctx, TOK_Ampersand);
        if (!op_token) op_token = Accept(ctx, TOK_Pipe);
        if (!op_token) op_token = Accept(ctx, TOK_Hat);
        if (!op_token) break;

        Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Plus:      op = BIN_OP_Add; break;
            case TOK_Minus:     op = BIN_OP_Subtract; break;
            case TOK_Ampersand: op = BIN_OP_BitAnd; break;
            case TOK_Pipe:      op = BIN_OP_BitOr; break;
            case TOK_Hat:       op = BIN_OP_BitXor; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = op;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseMultDivExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseShiftExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseAddSubExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_LtLt);
        if (!op_token) op_token = Accept(ctx, TOK_GtGt);
        if (!op_token) break;

        Binary_Op op;
        switch (op_token->type)
        {
            case TOK_LtLt:      op = BIN_OP_LeftShift; break;
            case TOK_GtGt:      op = BIN_OP_RightShift; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = op;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseAddSubExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseRangeExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseShiftExpr(ctx);
    if (!expr) return nullptr;

    if (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_PeriodPeriod);
        if (!op_token) return expr;

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = BIN_OP_Range;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseShiftExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, BIN_OP_Range);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseComparisonExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseRangeExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_EqEq);
        if (!op_token) op_token = Accept(ctx, TOK_NotEq);
        if (!op_token) op_token = Accept(ctx, TOK_Less);
        if (!op_token) op_token = Accept(ctx, TOK_LessEq);
        if (!op_token) op_token = Accept(ctx, TOK_Greater);
        if (!op_token) op_token = Accept(ctx, TOK_GreaterEq);
        if (!op_token) break;

        Binary_Op op;
        switch (op_token->type)
        {
            case TOK_EqEq:      op = BIN_OP_Equal; break;
            case TOK_NotEq:     op = BIN_OP_NotEqual; break;
            case TOK_Less:      op = BIN_OP_Less; break;
            case TOK_LessEq:    op = BIN_OP_LessEq; break;
            case TOK_Greater:   op = BIN_OP_Greater; break;
            case TOK_GreaterEq: op = BIN_OP_GreaterEq; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = op;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseShiftExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseLogicalExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseComparisonExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_AmpAmp);
        if (!op_token) op_token = Accept(ctx, TOK_PipePipe);
        if (!op_token) break;

        Binary_Op op;
        switch (op_token->type)
        {
            case TOK_AmpAmp:    op = BIN_OP_And; break;
            case TOK_PipePipe:  op = BIN_OP_Or; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Binary_Expr>(ctx, AST_BinaryExpr, op_token);
        bin_expr->binary_expr.op = op;
        bin_expr->binary_expr.left = expr;
        bin_expr->binary_expr.right = ParseComparisonExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseTernaryExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseLogicalExpr(ctx);
    if (!expr) return nullptr;

    const Token *qmark_tok = Accept(ctx, TOK_QuestionMark);
    if (!qmark_tok) return expr;

    Ast_Expr *true_expr = ParseLogicalExpr(ctx);
    if (!true_expr)
        Error(ctx, qmark_tok, "Expecting expression after ternary ?");

    const Token *colon_tok = ExpectAfterLast(ctx, TOK_Colon);

    Ast_Expr *false_expr = ParseLogicalExpr(ctx);
    if (!false_expr)
        Error(ctx, colon_tok, "Expecting expression after ternary :");

    Ast_Expr *ternary_expr = PushExpr<Ast_Ternary_Expr>(ctx, AST_TernaryExpr, qmark_tok);
    ternary_expr->ternary_expr.cond_expr = expr;
    ternary_expr->ternary_expr.true_expr = true_expr;
    ternary_expr->ternary_expr.false_expr = false_expr;

    return ternary_expr;
}

static Ast_Expr* ParseAssignmentExpr(Parser_Context *ctx)
{
    Ast_Expr *expr = ParseTernaryExpr(ctx);
    if (!expr) return nullptr;

    if (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Eq);
        if (!op_token) op_token = Accept(ctx, TOK_PlusEq);
        if (!op_token) op_token = Accept(ctx, TOK_MinusEq);
        if (!op_token) op_token = Accept(ctx, TOK_StarEq);
        if (!op_token) op_token = Accept(ctx, TOK_SlashEq);
        if (!op_token) op_token = Accept(ctx, TOK_PercentEq);
        if (!op_token) op_token = Accept(ctx, TOK_LtLtEq);
        if (!op_token) op_token = Accept(ctx, TOK_GtGtEq);
        if (!op_token) op_token = Accept(ctx, TOK_AmpEq);
        if (!op_token) op_token = Accept(ctx, TOK_PipeEq);
        if (!op_token) op_token = Accept(ctx, TOK_HatEq);
        if (!op_token) return expr;

        Assignment_Op op;
        switch (op_token->type)
        {
            case TOK_Eq:        op = AS_OP_Assign; break;
            case TOK_PlusEq:    op = AS_OP_AddAssign; break;
            case TOK_MinusEq:   op = AS_OP_SubtractAssign; break;
            case TOK_StarEq:    op = AS_OP_MultiplyAssign; break;
            case TOK_SlashEq:   op = AS_OP_DivideAssign; break;
            case TOK_PercentEq: op = AS_OP_ModuloAssign; break;
            case TOK_LtLtEq:    op = AS_OP_LeftShiftAssign; break;
            case TOK_GtGtEq:    op = AS_OP_RightShiftAssign; break;
            case TOK_AmpEq:     op = AS_OP_BitAndAssign; break;
            case TOK_PipeEq:    op = AS_OP_BitOrAssign; break;
            case TOK_HatEq:     op = AS_OP_BitXorAssign; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Expr *bin_expr = PushExpr<Ast_Assignment>(ctx, AST_AssignmentExpr, op_token);
        bin_expr->assignment.op = op;
        bin_expr->assignment.left = expr;
        bin_expr->assignment.right = ParseAssignmentExpr(ctx);

        if (!bin_expr->binary_expr.right)
            ErrorAssignmentExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Expr* ParseExpression(Parser_Context *ctx)
{
    /*
     * operator precedence (LR: left to right, RL: right to left)
     * RL: = += -= *= /= <<= >>=    assignment
     * LR: ?:                       ternary
     * LR: && ||                    logical and/or
     * LR: == != < > <= >=          comparison
     * LR: ..                       range expression
     * LR: << >>                    bit shift
     * LR: + - & | ^                add, sub, bit and/or/xor
     * LR: * / %                    mult, div, mod
     * LR: + - ~ !                  unary pos/neg, bit complement, logical not
     */
    Ast_Expr *expr = ParseAssignmentExpr(ctx);
    return expr;
}

static Ast_Node* ParseVarDeclExpr(Parser_Context *ctx);
static Ast_Node* ParseVarDeclStatement(Parser_Context *ctx);
static Ast_Node* ParseStatement(Parser_Context *ctx);

static Ast_Node* ParseBlockStatement(Parser_Context *ctx)
{
    const Token *block_tok = Accept(ctx, TOK_OpenBlock);
    if (!block_tok) return nullptr;

    Ast_Node *block_node = PushNode<Ast_Block_Stmt>(ctx, AST_BlockStmt, block_tok);
    do
    {
        if (Accept(ctx, TOK_CloseBlock))
            break;
        Ast_Node *stmt_node = ParseStatement(ctx);
        if (stmt_node)
        {
            PushNodeList(&block_node->block_stmt.statements, stmt_node);
        }
        else
        {
            ErrorInvalidToken(ctx, GetCurrentToken(ctx));
            GetNextToken(ctx);
        }
    } while (ContinueParsing(ctx));
    return block_node;
}

static Ast_Node* ParseIfStatement(Parser_Context *ctx)
{
    const Token *if_tok = Accept(ctx, TOK_If);
    if (!if_tok) return nullptr;

    Ast_Node *if_node = PushNode<Ast_If_Stmt>(ctx, AST_IfStmt, if_tok);
    Ast_Expr *cond_expr = ParseExpression(ctx);
    if (!cond_expr)
    {
        Error(ctx, "Expecting condition expression for if statement");
        GetNextToken(ctx);
    }
    Ast_Node *then_stmt = ParseStatement(ctx);
    if (!then_stmt)
        Error(ctx, "Expecting statement after if");

    Ast_Node *else_stmt = nullptr;
    if (Accept(ctx, TOK_Else))
    {
        else_stmt = ParseStatement(ctx);
        if (!else_stmt)
        {
            Error(ctx, "Expecting statement after else");
        }
    }
    if_node->if_stmt.cond_expr = cond_expr;
    if_node->if_stmt.then_stmt = then_stmt;
    if_node->if_stmt.else_stmt = else_stmt;
    return if_node;
}

static Ast_Node* ParseWhileStatement(Parser_Context *ctx)
{
    const Token *while_tok = Accept(ctx, TOK_While);
    if (!while_tok) return nullptr;

    // TODO(henrik): Parsing if and while should take the expression in
    // parentheses as 'special' syntax and not let the expression parsing
    // deal with them. This would avoid the problem where if (expr) + 5; could
    // be interpreted differently by a human and the parser.

    Ast_Node *while_node = PushNode<Ast_While_Stmt>(ctx, AST_WhileStmt, while_tok);
    Ast_Expr *cond_expr = ParseExpression(ctx);
    if (!cond_expr)
        Error(ctx, "Expecting condition expression after while");

    Ast_Node *loop_stmt = ParseStatement(ctx);
    if (!loop_stmt)
        Error(ctx, "Expecting statement after while");

    while_node->while_stmt.cond_expr = cond_expr;
    while_node->while_stmt.loop_stmt = loop_stmt;

    return while_node;
}

static Ast_Node* ParseForStatement(Parser_Context *ctx)
{
    const Token *for_tok = Accept(ctx, TOK_For);
    if (!for_tok) return nullptr;

    if (Accept(ctx, TOK_OpenParent))
    {
        Ast_Node *for_node = PushNode<Ast_For_Stmt>(ctx, AST_ForStmt, for_tok);

        Ast_Node *init_stmt = ParseVarDeclStatement(ctx);
        Ast_Expr *init_expr = nullptr;
        if (!init_stmt)
        {
            init_expr = ParseExpression(ctx);
            Expect(ctx, TOK_Semicolon);
            if (!init_expr)
            {
                Error(ctx, "Expecting for statement init expression");
                GetNextToken(ctx);
            }
        }
        Ast_Expr *cond_expr = ParseExpression(ctx);
        Expect(ctx, TOK_Semicolon);

        Ast_Expr *incr_expr = ParseExpression(ctx);
        ExpectAfterLast(ctx, TOK_CloseParent);

        Ast_Node *loop_stmt = ParseStatement(ctx);
        if (!loop_stmt)
        {
            Error(ctx, "Expecting statement after for");
        }

        for_node->for_stmt.init_stmt = init_stmt;
        for_node->for_stmt.init_expr = init_expr;
        for_node->for_stmt.cond_expr = cond_expr;
        for_node->for_stmt.incr_expr = incr_expr;
        for_node->for_stmt.loop_stmt = loop_stmt;
        return for_node;
    }
    else
    {
        Ast_Node *for_node = PushNode<Ast_Range_For_Stmt>(ctx, AST_RangeForStmt, for_tok);

        Ast_Node *init_stmt = ParseVarDeclExpr(ctx);
        Ast_Expr *init_expr = nullptr;
        if (!init_stmt)
        {
            init_expr = ParseExpression(ctx);
            if (!init_expr)
            {
                Error(ctx, "Expecting range expression after for");
            }
        }
        Ast_Node *loop_stmt = ParseStatement(ctx);
        if (!loop_stmt)
        {
            Error(ctx, "Expecting statement after for");
        }

        for_node->range_for_stmt.init_stmt = init_stmt;
        for_node->range_for_stmt.init_expr = init_expr;
        for_node->range_for_stmt.loop_stmt = loop_stmt;
        return for_node;
    }
    return nullptr;
}

static Ast_Node* ParseReturnStatement(Parser_Context *ctx)
{
    const Token *return_tok = Accept(ctx, TOK_Return);
    if (!return_tok) return nullptr;

    Ast_Node *return_node = PushNode<Ast_Return_Stmt>(ctx, AST_ReturnStmt, return_tok);
    Ast_Expr *expr = ParseExpression(ctx);
    return_node->return_stmt.expr = expr;
    ExpectAfterLast(ctx, TOK_Semicolon);
    return return_node;
}

static Ast_Node* ParseVarDeclExpr(Parser_Context *ctx)
{
    const Token *ident_tok = GetCurrentToken(ctx);
    if (ident_tok->type != TOK_Identifier)
        return nullptr;

    const Token *peek = PeekNextToken(ctx);
    if (peek->type != TOK_Comma &&
        peek->type != TOK_Colon &&
        peek->type != TOK_ColonEq)
    {
        return nullptr;
    }

    Accept(ctx, TOK_Identifier);

    Ast_Node *var_decl = PushNode<Ast_Variable_Decl>(ctx, AST_VariableDecl, ident_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    var_decl->variable_decl.names.name = name;
    var_decl->variable_decl.names.file_loc = ident_tok->file_loc;

    Ast_Variable_Decl_Names *prev = &var_decl->variable_decl.names;
    while (Accept(ctx, TOK_Comma))
    {
        ident_tok = Accept(ctx, TOK_Identifier);
        if (!ident_tok)
        {
            Error(ctx, "Expecting variable name after comma");
            break;
        }
        Ast_Variable_Decl_Names *next = PushStruct<Ast_Variable_Decl_Names>(&ctx->ast->arena);
        *next = { };
        next->name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
        next->file_loc = ident_tok->file_loc;

        prev->next = next;
        prev = next;
    }

    Ast_Node *type = nullptr;
    Ast_Expr *init_expr = nullptr;
    if (Accept(ctx, TOK_Colon))
    {
        type = ParseType(ctx);
        if (!type)
        {
            Error(ctx, "Expecting type for variable");
        }

        if (Accept(ctx, TOK_Eq))
        {
            init_expr = ParseExpression(ctx);
            if (!init_expr)
            {
                Error(ctx, "Expecting initializing expression for variable");
            }
        }
    }
    else
    {
        Expect(ctx, TOK_ColonEq);
        init_expr = ParseExpression(ctx);
        if (!init_expr)
        {
            Error(ctx, "Expecting initializing expression for variable");
        }
    }
    var_decl->variable_decl.type = type;
    var_decl->variable_decl.init_expr = init_expr;
    return var_decl;
}

static Ast_Node* ParseVarDeclStatement(Parser_Context *ctx)
{
    // NOTE(henrik): Needs to be parsed before expression because of ambiguous
    // grammar.
    Ast_Node *var_decl = ParseVarDeclExpr(ctx);
    if (!var_decl) return nullptr;

    ExpectAfterLast(ctx, TOK_Semicolon);
    return var_decl;
}

static Ast_Node* ParseBreakOrContinueStatement(Parser_Context *ctx)
{
    const Token *break_tok = Accept(ctx, TOK_Break);
    if (break_tok)
    {
        Ast_Node *break_node = PushNode<Ast_Break>(ctx, AST_BreakStmt, break_tok);
        ExpectAfterLast(ctx, TOK_Semicolon);
        return break_node;
    }
    const Token *cont_tok = Accept(ctx, TOK_Continue);
    if (cont_tok)
    {
        Ast_Node *cont = PushNode<Ast_Continue>(ctx, AST_ContinueStmt, cont_tok);
        ExpectAfterLast(ctx, TOK_Semicolon);
        return cont;
    }
    return nullptr;
}

static Ast_Node* ParseExprStatement(Parser_Context *ctx)
{
    const Token *tok = GetCurrentToken(ctx);
    Ast_Expr *expr = ParseExpression(ctx);
    if (!expr) return nullptr;

    ExpectAfterLast(ctx, TOK_Semicolon);
    Ast_Node *stmt = PushNode<Ast_Expr_Stmt>(ctx, AST_ExpressionStmt, tok);
    stmt->expr_stmt.expr = expr;
    return stmt;
}

static Ast_Node* ParseStatement(Parser_Context *ctx)
{
    Ast_Node *stmt = ParseBlockStatement(ctx);
    if (!stmt) stmt = ParseIfStatement(ctx);
    if (!stmt) stmt = ParseWhileStatement(ctx);
    if (!stmt) stmt = ParseForStatement(ctx);
    if (!stmt) stmt = ParseReturnStatement(ctx);
    if (!stmt) stmt = ParseBreakOrContinueStatement(ctx);
    if (!stmt) stmt = ParseVarDeclStatement(ctx);
    if (!stmt) stmt = ParseExprStatement(ctx);
    return stmt;
}

static void ParseParameters(Parser_Context *ctx, Ast_Node_List *parameters)
{
    bool first = true;
    do
    {
        const Token *ident_tok = Accept(ctx, TOK_Identifier);
        if (ident_tok)
        {
            Ast_Node *param_node = PushNode<Ast_Parameter>(ctx, AST_Parameter, ident_tok);
            Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
            param_node->parameter.name = name;

            Expect(ctx, TOK_Colon);
            Ast_Node *type_node = ParseType(ctx);
            param_node->parameter.type = type_node;

            PushNodeList(parameters, param_node);
            first = false;
        }
        else if (first)
        {
            break;
        }
        else
        {
            Error(ctx, "Expecting parameter name");
            break;
        }
    } while (Accept(ctx, TOK_Comma) && ContinueParsing(ctx));
}

static Ast_Node* ParseFunction(Parser_Context *ctx, const Token *ident_tok)
{
    if (!Accept(ctx, TOK_OpenParent)) return nullptr;

    Ast_Node *func_def = PushNode<Ast_Function_Def>(ctx, AST_FunctionDef, ident_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    func_def->function_def.name = name;

    ParseParameters(ctx, &func_def->function_def.parameters);
    ExpectAfterLast(ctx, TOK_CloseParent);
    if (Accept(ctx, TOK_Colon))
    {
        // NOTE(henrik): The return type node can be nullptr. This means
        // that the type will be inferred later. But, as this leads to
        // ambiguous syntax for inferred return type and no return type, the
        // syntax should be changed. E.g. maybe
        //   func_name :: (...) : *
        // will have inferred return type.
        Ast_Node *return_type = ParseType(ctx);
        func_def->function_def.return_type = return_type;
        if (!return_type)
        {
            Error(ctx, "Expecting function return type");
            if (GetCurrentToken(ctx)->type != TOK_OpenBlock)
                GetNextToken(ctx);
        }
    }
    Ast_Node *body = ParseBlockStatement(ctx);
    if (!body)
    {
        Error(ctx, "Expecting function body");
    }
    func_def->function_def.body = body;
    return func_def;
}

static Ast_Node* ParseStructMember(Parser_Context *ctx)
{
    const Token *ident_tok = Accept(ctx, TOK_Identifier);
    if (!ident_tok) return nullptr;

    ExpectAfterLast(ctx, TOK_Colon);

    Ast_Node *member = PushNode<Ast_Struct_Member>(ctx, AST_StructMember, ident_tok);
    Ast_Node *type = ParseType(ctx);

    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
    member->struct_member.name = name;
    member->struct_member.type = type;

    ExpectAfterLast(ctx, TOK_Semicolon);

    return member;
}

static Ast_Node* ParseTypealias(Parser_Context *ctx, const Token *ident_tok)
{
    const Token *typealias_tok = Accept(ctx, TOK_Typealias);
    if (!typealias_tok) return nullptr;

    Ast_Node *typealias = PushNode<Ast_Typealias>(ctx, AST_Typealias, typealias_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    typealias->typealias.name = name;
    typealias->typealias.type = ParseType(ctx);

    if (!typealias->typealias.type)
    {
        Error(ctx, "Expecting type for typealias");
    }
    ExpectAfterLast(ctx, TOK_Semicolon);

    return typealias;
}

static Ast_Node* ParseStruct(Parser_Context *ctx, const Token *ident_tok)
{
    const Token *struct_tok = Accept(ctx, TOK_Struct);
    if (!struct_tok) return nullptr;

    Ast_Node *struct_def = PushNode<Ast_Struct_Def>(ctx, AST_StructDef, ident_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    struct_def->struct_def.name = name;

    Expect(ctx, TOK_OpenBlock);
    while (ContinueParsing(ctx))
    {
        Ast_Node *member = ParseStructMember(ctx);
        if (!member) break;

        PushNodeList(&struct_def->struct_def.members, member);
    }
    ExpectAfterLast(ctx, TOK_CloseBlock);
    return struct_def;
}

static Ast_Node* ParseNamedImport(Parser_Context *ctx, const Token *ident_tok)
{
    const Token *import_tok = Accept(ctx, TOK_Import);
    if (!import_tok) return nullptr;

    const Token *module_name_tok = ExpectAfterLast(ctx, TOK_StringLit);
    if (!module_name_tok)
    {
        Error(ctx, "Expecting module name as string literal");
    }
    ExpectAfterLast(ctx, TOK_Semicolon);

    Ast_Node *import_node = PushNode<Ast_Import>(ctx, AST_Import, import_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    String mod_name_str = ConvertString(ctx,
            module_name_tok->value, module_name_tok->value_end);
    import_node->import.name = name;
    import_node->import.module_name = mod_name_str;

    return import_node;
}

static Ast_Node* ParseGlobalImport(Parser_Context *ctx)
{
    const Token *import_tok = Accept(ctx, TOK_Import);
    if (!import_tok) return nullptr;

    const Token *module_name_tok = ExpectAfterLast(ctx, TOK_StringLit);
    if (!module_name_tok)
    {
        Error(ctx, "Expecting module name as string literal");
    }
    ExpectAfterLast(ctx, TOK_Semicolon);

    Ast_Node *import_node = PushNode<Ast_Import>(ctx, AST_Import, import_tok);
    String mod_name_str = ConvertString(ctx,
            module_name_tok->value, module_name_tok->value_end);
    import_node->import.name = { };
    import_node->import.module_name = mod_name_str;

    return import_node;
}

static Ast_Node* ParseForeignFunction(Parser_Context *ctx, const Token *ident_tok)
{
    if (!Accept(ctx, TOK_OpenParent)) return nullptr;

    Ast_Node *func_decl = PushNode<Ast_Function_Decl>(ctx, AST_FunctionDecl, ident_tok);
    Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
    func_decl->function_decl.name = name;

    ParseParameters(ctx, &func_decl->function_decl.parameters);
    ExpectAfterLast(ctx, TOK_CloseParent);
    if (Accept(ctx, TOK_Colon))
    {
        // NOTE(henrik): The return type node can be nullptr. This means
        // that the type will be inferred later. But, as this leads to
        // ambiguous syntax for inferred return type and no return type, the
        // syntax should be changed. E.g. maybe
        //   func_name :: (...) : *
        // will have inferred return type.
        Ast_Node *return_type = ParseType(ctx);
        func_decl->function_decl.return_type = return_type;
        if (!return_type)
        {
            Error(ctx, "Expecting function return type");
        }
    }
    else
    {
        Error(ctx, "Return type must be specified for foreign functions");
    }
    Expect(ctx, TOK_Semicolon);
    return func_decl;
}

static Ast_Node* ParseForeignStmt(Parser_Context *ctx)
{
    const Token *token = GetCurrentToken(ctx);
    if (token->type != TOK_Identifier)
        return nullptr;

    const Token *peek = PeekNextToken(ctx);
    if (peek->type != TOK_ColonColon)
        return nullptr;

    const Token *ident_tok = Accept(ctx, TOK_Identifier);
    Accept(ctx, TOK_ColonColon);

    Ast_Node *stmt = ParseNamedImport(ctx, ident_tok);
    if (!stmt) stmt = ParseStruct(ctx, ident_tok);
    if (!stmt) stmt = ParseForeignFunction(ctx, ident_tok);
    return stmt;
}

static Ast_Node* ParseForeignBlock(Parser_Context *ctx)
{
    const Token *foreign_tok = Accept(ctx, TOK_Foreign);
    if (!foreign_tok) return nullptr;

    Ast_Node *foreign_block = PushNode<Ast_Foreign_Block>(ctx, AST_ForeignBlock, foreign_tok);

    Expect(ctx, TOK_OpenBlock);
    while (ContinueParsing(ctx))
    {
        Ast_Node *stmt = ParseForeignStmt(ctx);
        if (!stmt)
        break;

        PushNodeList(&foreign_block->foreign.statements, stmt);
    }
    Expect(ctx, TOK_CloseBlock);

    return foreign_block;
}

static Ast_Node* ParseTopLevelNamedStmt(Parser_Context *ctx)
{
    const Token *token = GetCurrentToken(ctx);
    if (token->type != TOK_Identifier)
        return nullptr;

    const Token *peek = PeekNextToken(ctx);
    if (peek->type != TOK_ColonColon)
        return nullptr;

    const Token *ident_tok = Accept(ctx, TOK_Identifier);
    Accept(ctx, TOK_ColonColon);

    Ast_Node *stmt = ParseNamedImport(ctx, ident_tok);
    if (!stmt) stmt = ParseTypealias(ctx, ident_tok);
    if (!stmt) stmt = ParseStruct(ctx, ident_tok);
    if (!stmt) stmt = ParseFunction(ctx, ident_tok);
    return stmt;
}

static Ast_Node* ParseTopLevelStmt(Parser_Context *ctx)
{
    Ast_Node *stmt = ParseGlobalImport(ctx);
    if (!stmt) stmt = ParseForeignBlock(ctx);
    if (!stmt) stmt = ParseVarDeclStatement(ctx);
    if (!stmt) stmt = ParseTopLevelNamedStmt(ctx);
    return stmt;
}

b32 Parse(Parser_Context *ctx)
{
    ASSERT(ctx && ctx->ast);
    ctx->ast->root = PushNode<Ast_Top_Level>(ctx, AST_TopLevel, GetCurrentToken(ctx));
    Ast_Node *root = ctx->ast->root;

    while (ContinueParsing(ctx))
    {
        Ast_Node *stmt = ParseTopLevelStmt(ctx);
        if (stmt)
        {
            PushNodeList(&root->top_level.statements, stmt);
        }
        else if (ContinueParsing(ctx))
        {
            ErrorInvalidToken(ctx, GetCurrentToken(ctx));
            GetNextToken(ctx);
        }
    }
    return HasError(ctx->comp_ctx);
}

} // hplang
