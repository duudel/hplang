
#include "parser.h"
#include "ast_types.h"
#include "compiler.h"
#include "assert.h"

#include <cstdlib>
#include <cerrno>

#if 0
#define TRACE(x) {fprintf(stderr, "%s\n", #x); fflush(stderr);}
#define TRACE_x(x) {fprintf(stderr, "%s : %d:%d\n", #x,\
        GetCurrentToken(ctx)->file_loc.line,\
        GetCurrentToken(ctx)->file_loc.column); fflush(stderr);}
#else
#define TRACE(x)
#endif

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

void UnlinkAst(Parser_Context *ctx)
{
    ctx->ast->tokens = ctx->tokens;
    ctx->ast = nullptr;
    ctx->tokens = nullptr;
}

void FreeParserContext(Parser_Context *ctx)
{
    FreeMemoryArena(&ctx->temp_arena);
    if (ctx->ast)
    {
        FreeAst(ctx->ast);
        ctx->ast = nullptr;
    }
}

// Parsing
// -------

static void Error(Parser_Context *ctx, const Token *token, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf(err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, token->file_loc);
}

static void ErrorInvalidToken(Parser_Context *ctx, const Token *token)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf(err_ctx->file, "Invalid token ");
    PrintTokenValue(err_ctx->file, token);
    fprintf(err_ctx->file, "\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, token->file_loc);
}

static void ErrorExpected(Parser_Context *ctx,
        const Token *token,
        const char *expected_token)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    fprintf(err_ctx->file, "Expecting %s\n", expected_token);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, token->file_loc);
}

static void ErrorExpectedAtEnd(Parser_Context *ctx,
        const Token *token,
        const char *expected_token)
{
    File_Location file_loc = token->file_loc;

    // NOTE(henrik): This will fail, if the token extends multiple lines.
    // This almost never happens though. Currently only with multiline strings
    // A better solution might be to explicitly store the end of token
    // as another File_Location in the token.
    file_loc.column += (token->value_end - token->value);

    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, file_loc);
    PrintFileLocation(err_ctx->file, file_loc);
    fprintf(err_ctx->file, "Expecting %s\n", expected_token);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, file_loc);
}

static void ErrorUnexpectedEOF(Parser_Context *ctx)
{
    const Token *last_token = &ctx->tokens->array.data[ctx->tokens->array.count - 1];
    Error(ctx, last_token, "Unexpected end of file");
}

static void ErrorBinaryExprRHS(Parser_Context *ctx, const Token *token, Ast_Binary_Op op)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    const char *op_str = "";
    switch (op)
    {
        case AST_OP_Add:        op_str = "+"; break;
        case AST_OP_Subtract:   op_str = "-"; break;
        case AST_OP_Multiply:   op_str = "*"; break;
        case AST_OP_Divide:     op_str = "/"; break;
        case AST_OP_Modulo:     op_str = "-"; break;
        case AST_OP_BitAnd:     op_str = "&"; break;
        case AST_OP_BitOr:      op_str = "|"; break;
        case AST_OP_BitXor:     op_str = "^"; break;

        case AST_OP_And:        op_str = "&&"; break;
        case AST_OP_Or:         op_str = "||"; break;

        case AST_OP_Equal:      op_str = "=="; break;
        case AST_OP_NotEqual:   op_str = "!="; break;
        case AST_OP_Less:       op_str = "<"; break;
        case AST_OP_LessEq:     op_str = "<="; break;
        case AST_OP_Greater:    op_str = ">"; break;
        case AST_OP_GreaterEq:  op_str = ">="; break;

        case AST_OP_Range:      op_str = ".."; break;

        // NOTE(henrik): These are not used here, but as they are binary ops,
        // need to have them here to suppress warnings.
        case AST_OP_Access:     op_str = "."; break;
        case AST_OP_Subscript:  op_str = "[]"; break;
    }
    fprintf(err_ctx->file, "Expecting right hand side operand for operator %s\n", op_str);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, token->file_loc);
}

static void ErrorAssignmentExprRHS(Parser_Context *ctx, const Token *token, Ast_Assignment_Op op)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, token->file_loc);
    PrintFileLocation(err_ctx->file, token->file_loc);
    const char *op_str = "";
    switch (op)
    {
        case AST_OP_Assign:             op_str = "="; break;
        case AST_OP_AddAssign:          op_str = "+="; break;
        case AST_OP_SubtractAssign:     op_str = "-="; break;
        case AST_OP_MultiplyAssign:     op_str = "*="; break;
        case AST_OP_DivideAssign:       op_str = "/="; break;
        case AST_OP_ModuloAssign:       op_str = "%="; break;
        case AST_OP_BitAndAssign:       op_str = "&="; break;
        case AST_OP_BitOrAssign:        op_str = "|="; break;
        case AST_OP_BitXorAssign:       op_str = "^="; break;
        case AST_OP_ComplementAssign:   op_str = "~="; break;
    }
    fprintf(err_ctx->file, "Expecting right hand side operand for operator %s\n", op_str);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, token->file_loc);
}


static Token eof_token = { TOK_EOF };

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

static Ast_Node* PushNode(Parser_Context *ctx,
        Ast_Node_Type node_type, const Token *token)
{
    return PushAstNode(ctx->ast, node_type, token);
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
    if (s != end)
        ; // error
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

static Ast_Node* ParseLiteralExpr(Parser_Context *ctx)
{
    const Token *token = Accept(ctx, TOK_Null);
    if (token)
    {
        return PushNode(ctx, AST_Null, token);
    }
    token = Accept(ctx, TOK_TrueLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_BoolLiteral, token);
        literal->expression.bool_literal.value = true;
        return literal;
    }
    token = Accept(ctx, TOK_FalseLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_BoolLiteral, token);
        literal->expression.bool_literal.value = false;
        return literal;
    }
    token = Accept(ctx, TOK_IntegerLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_IntLiteral, token);
        literal->expression.int_literal.value = ConvertInt(token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_Float32Lit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_Float32Literal, token);
        literal->expression.float32_literal.value = ConvertFloat(ctx, token);
        return literal;
    }
    token = Accept(ctx, TOK_Float64Lit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_Float64Literal, token);
        literal->expression.float64_literal.value = ConvertFloat(ctx, token);
        return literal;
    }
    token = Accept(ctx, TOK_CharLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_CharLiterla, token);
        literal->expression.char_literal.value = ConvertChar(ctx, token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_StringLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_StringLiteral, token);
        literal->expression.string_literal.value = ConvertString(ctx, token->value, token->value_end);
        return literal;
    }
    TRACE(ParseLiteralExpr_not_literal);
    return nullptr;
}

static Ast_Node* ParsePrefixOperator(Parser_Context *ctx)
{
    TRACE(ParsePrefixOperator);
    const Token *op_token = Accept(ctx, TOK_Plus);
    if (!op_token) op_token = Accept(ctx, TOK_Minus);
    if (!op_token) op_token = Accept(ctx, TOK_Tilde);
    if (!op_token) op_token = Accept(ctx, TOK_Bang);
    if (!op_token) op_token = Accept(ctx, TOK_Ampersand);
    if (!op_token) op_token = Accept(ctx, TOK_At);
    if (!op_token) return nullptr;

    Ast_Node *pre_op = PushNode(ctx, AST_UnaryExpr, op_token);

    Ast_Unary_Op op;
    switch (op_token->type)
    {
        case TOK_Plus:      op = AST_OP_Positive; break;
        case TOK_Minus:     op = AST_OP_Negative; break;
        case TOK_Tilde:     op = AST_OP_Complement; break;
        case TOK_Bang:      op = AST_OP_Not; break;
        case TOK_Ampersand: op = AST_OP_Address; break;
        case TOK_At:        op = AST_OP_Deref; break;
        default: INVALID_CODE_PATH;
    }
    pre_op->expression.unary_expr.op = op;
    pre_op->expression.unary_expr.expr = nullptr;
    return pre_op;
}

static Ast_Node* ParseExpression(Parser_Context *ctx);

static Ast_Node* ParsePostfixOperator(Parser_Context *ctx, Ast_Node *factor)
{
    TRACE(ParsePostfixOperator);
    const Token *op_token = Accept(ctx, TOK_Period);
    if (op_token)
    {
        Ast_Node *access_expr = PushNode(ctx, AST_BinaryExpr, op_token);

        const Token *ident_tok = Accept(ctx, TOK_Identifier);
        if (ident_tok)
        {
            Ast_Node *member_ref  = PushNode(ctx, AST_VariableRef, ident_tok);
            Name name = PushName(&ctx->ast->arena,
                    ident_tok->value, ident_tok->value_end);
            member_ref->expression.variable_ref.name = name;

            access_expr->expression.binary_expr.op = AST_OP_Access;
            access_expr->expression.binary_expr.left = factor;
            access_expr->expression.binary_expr.right = member_ref;
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
        Ast_Node *subscript_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        subscript_expr->expression.binary_expr.op = AST_OP_Access;
        subscript_expr->expression.binary_expr.left = factor;
        subscript_expr->expression.binary_expr.right = ParseExpression(ctx);
        if (!subscript_expr->expression.binary_expr.right)
            Error(ctx, "Expecting subscript expression");
        Expect(ctx, TOK_CloseBracket);
    }
    return nullptr;
}

static Ast_Node* ParseFunctionArgs(Parser_Context *ctx)
{
    TRACE(ParseFunctionArgs);
    Ast_Node *args = PushNode(ctx, AST_FunctionCallArgs, GetCurrentToken(ctx));

    Ast_Node *arg = ParseExpression(ctx);
    if (arg)
    {
        PushNodeList(&args->node_list, arg);
        while (Accept(ctx, TOK_Comma) && ContinueParsing(ctx))
        {
            Ast_Node *arg = ParseExpression(ctx);
            if (!arg)
            {
                Error(ctx, "Expecting function argument after comma");
                break;
            }
            PushNodeList(&args->node_list, arg);
        }
    }
    return args;
}

static Ast_Node* ParseFactorExpr(Parser_Context *ctx)
{
    TRACE(ParseFactor);
    Ast_Node *pre_op = ParsePrefixOperator(ctx);

    Ast_Node *factor = ParseLiteralExpr(ctx);

    if (!factor)
    {
        const Token *ident_tok = Accept(ctx, TOK_Identifier);
        if (ident_tok)
        {
            TRACE(ParseFactor_identifier);
            Name name = PushName(&ctx->ast->arena, ident_tok->value, ident_tok->value_end);
            // NOTE(henrik): Only accept function call for identifiers.
            // Is there any benefit to accept same syntax for other types of
            // factors? Maybe even for error reporting reasons?
            // Also, if we want to store functions into structures, we would not be
            // able to just call them like "thing.func()"
            if (Accept(ctx, TOK_OpenParent))
            {
                TRACE(ParseFactor_func_call);
                factor = PushNode(ctx, AST_FunctionCall, ident_tok);
                Ast_Node *args = ParseFunctionArgs(ctx);
                factor->expression.function_call.name = name;
                factor->expression.function_call.args = args;
                ExpectAfterLast(ctx, TOK_CloseParent);
            }
            else
            {
                TRACE(ParseFactor_var_ref);
                factor = PushNode(ctx, AST_VariableRef, ident_tok);
                factor->expression.variable_ref.name = name;
            }
        }
    }

    if (!factor)
    {
        const Token *parent_tok = Accept(ctx, TOK_OpenParent);
        if (parent_tok)
        {
            TRACE(ParseFactor_parent);
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
    Ast_Node *post_op = ParsePostfixOperator(ctx, factor);
    if (post_op)
    {
        factor = post_op;
    }

    if (pre_op)
    {
        TRACE(ParseFactor_pre_op);
        pre_op->expression.unary_expr.expr = factor;
        factor = pre_op;
    }

    TRACE(ParseFactor_end);
    return factor;
}

static Ast_Node* ParseMultDivExpr(Parser_Context *ctx)
{
    TRACE(ParseMultDivExpr);
    Ast_Node *expr = ParseFactorExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Star);
        if (!op_token) op_token = Accept(ctx, TOK_Slash);
        if (!op_token) op_token = Accept(ctx, TOK_Percent);
        if (!op_token) break;

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Star:      op = AST_OP_Multiply; break;
            case TOK_Slash:     op = AST_OP_Divide; break;
            case TOK_Percent:   op = AST_OP_Modulo; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseFactorExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseAddSubExpr(Parser_Context *ctx)
{
    TRACE(ParseAddSubExpr);
    Ast_Node *expr = ParseMultDivExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Plus);
        if (!op_token) op_token = Accept(ctx, TOK_Minus);
        if (!op_token) op_token = Accept(ctx, TOK_Ampersand);
        if (!op_token) op_token = Accept(ctx, TOK_Pipe);
        if (!op_token) op_token = Accept(ctx, TOK_Hat);
        if (!op_token) break;

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Plus:      op = AST_OP_Add; break;
            case TOK_Minus:     op = AST_OP_Subtract; break;
            case TOK_Ampersand: op = AST_OP_BitAnd; break;
            case TOK_Pipe:      op = AST_OP_BitOr; break;
            case TOK_Hat:       op = AST_OP_BitXor; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseMultDivExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseRangeExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseAddSubExpr(ctx);
    if (!expr) return nullptr;

    if (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_PeriodPeriod);
        if (!op_token) return expr;

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = AST_OP_Range;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseAddSubExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, AST_OP_Range);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseLogicalExpr(Parser_Context *ctx)
{
    //Ast_Node *expr = ParseShiftExpr(ctx);
    //Ast_Node *expr = ParseAddSubExpr(ctx);
    Ast_Node *expr = ParseRangeExpr(ctx);
    if (!expr) return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_AmpAmp);
        if (!op_token) op_token = Accept(ctx, TOK_PipePipe);
        if (!op_token) break;

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_AmpAmp:    op = AST_OP_And; break;
            case TOK_PipePipe:  op = AST_OP_Or; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseAddSubExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseComparisonExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseLogicalExpr(ctx);
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

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_EqEq:      op = AST_OP_Equal; break;
            case TOK_NotEq:     op = AST_OP_NotEqual; break;
            case TOK_Less:      op = AST_OP_Less; break;
            case TOK_LessEq:    op = AST_OP_LessEq; break;
            case TOK_Greater:   op = AST_OP_Greater; break;
            case TOK_GreaterEq: op = AST_OP_GreaterEq; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseLogicalExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorBinaryExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseAssignmentExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseComparisonExpr(ctx);
    if (!expr) return nullptr;

    if (ContinueParsing(ctx))
    {
        const Token *op_token = Accept(ctx, TOK_Eq);
        if (!op_token) op_token = Accept(ctx, TOK_PlusEq);
        if (!op_token) op_token = Accept(ctx, TOK_MinusEq);
        if (!op_token) op_token = Accept(ctx, TOK_StarEq);
        if (!op_token) op_token = Accept(ctx, TOK_SlashEq);
        if (!op_token) op_token = Accept(ctx, TOK_PercentEq);
        if (!op_token) op_token = Accept(ctx, TOK_AmpEq);
        if (!op_token) op_token = Accept(ctx, TOK_PipeEq);
        if (!op_token) op_token = Accept(ctx, TOK_HatEq);
        if (!op_token) op_token = Accept(ctx, TOK_TildeEq);
        if (!op_token) return expr;

        Ast_Assignment_Op op;
        switch (op_token->type)
        {
            case TOK_Eq:        op = AST_OP_Assign; break;
            case TOK_PlusEq:    op = AST_OP_AddAssign; break;
            case TOK_MinusEq:   op = AST_OP_SubtractAssign; break;
            case TOK_StarEq:    op = AST_OP_MultiplyAssign; break;
            case TOK_SlashEq:   op = AST_OP_DivideAssign; break;
            case TOK_PercentEq: op = AST_OP_ModuloAssign; break;
            case TOK_AmpEq:     op = AST_OP_BitAndAssign; break;
            case TOK_PipeEq:    op = AST_OP_BitOrAssign; break;
            case TOK_HatEq:     op = AST_OP_BitXorAssign; break;
            case TOK_TildeEq:   op = AST_OP_ComplementAssign; break;
            default: INVALID_CODE_PATH;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_AssignmentExpr, op_token);
        bin_expr->expression.assignment.op = op;
        bin_expr->expression.assignment.left = expr;
        bin_expr->expression.assignment.right = ParseAssignmentExpr(ctx);

        if (!bin_expr->expression.binary_expr.right)
            ErrorAssignmentExprRHS(ctx, op_token, op);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseExpression(Parser_Context *ctx)
{
    TRACE(ParseExpression);
    /*
     * operator precedence (LR: left to right, RL: right to left)
     * RL: = += -= *= /=        assignment
     * LR: == != < > <= >=      comparison
     * LR: && ||                logical and/or
     * LR: >> <<                bit shift
     * LR: + - & | ^            add, sub, bit and/or/xor
     * LR: * / %                mult, div, mod
     * LR: + - ~                unary pos/neg, bit complement
     */
    Ast_Node *expr = ParseAssignmentExpr(ctx);
    TRACE(ParseExpression_end);
    return expr;
}

static Ast_Node* ParseVarDeclExpr(Parser_Context *ctx);
static Ast_Node* ParseVarDeclStatement(Parser_Context *ctx);
static Ast_Node* ParseStatement(Parser_Context *ctx);

static Ast_Node* ParseBlockStatement(Parser_Context *ctx)
{
    TRACE(ParseBlockStatement);
    const Token *block_tok = Accept(ctx, TOK_OpenBlock);
    if (!block_tok) return nullptr;

    Ast_Node *block_node = PushNode(ctx, AST_BlockStmt, block_tok);
    do
    {
        if (Accept(ctx, TOK_CloseBlock))
            break;
        Ast_Node *stmt_node = ParseStatement(ctx);
        if (stmt_node)
        {
            PushNodeList(&block_node->node_list, stmt_node);
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
    TRACE(ParseIfStatement);
    const Token *if_tok = Accept(ctx, TOK_If);
    if (!if_tok) return nullptr;

    Ast_Node *if_node = PushNode(ctx, AST_IfStmt, if_tok);
    Ast_Node *expr = ParseExpression(ctx);
    if (!expr)
    {
        Error(ctx, "Expecting condition expression for if statement");
        GetNextToken(ctx);
    }
    Ast_Node *true_stmt = ParseStatement(ctx);
    if (!true_stmt)
    {
        Error(ctx, "Expecting statement after if");
    }

    Ast_Node *false_stmt = nullptr;
    if (Accept(ctx, TOK_Else))
    {
        false_stmt = ParseStatement(ctx);
        if (!false_stmt)
        {
            Error(ctx, "Expecting statement after else");
        }
    }
    if_node->if_stmt.condition_expr = expr;
    if_node->if_stmt.true_stmt = true_stmt;
    if_node->if_stmt.false_stmt = false_stmt;
    return if_node;
}

static Ast_Node* ParseForStatement(Parser_Context *ctx)
{
    TRACE(ParseForStatement);
    const Token *for_tok = Accept(ctx, TOK_For);
    if (!for_tok) return nullptr;

    Ast_Node *for_node = PushNode(ctx, AST_ForStmt, for_tok);
    if (Accept(ctx, TOK_OpenParent))
    {
        TRACE(ParseForStatement_three_part);
        Ast_Node *init = ParseVarDeclStatement(ctx);
        if (!init)
        {
            init = ParseExpression(ctx);
            Expect(ctx, TOK_Semicolon);
        }
        if (!init)
        {
            Error(ctx, "Expecting for init expression");
            GetNextToken(ctx);
        }
        Ast_Node *cond = ParseExpression(ctx);
        Expect(ctx, TOK_Semicolon);

        Ast_Node *increment = ParseExpression(ctx);
        ExpectAfterLast(ctx, TOK_CloseParent);

        for_node->for_stmt.init_expr = init;
        for_node->for_stmt.condition_expr = cond;
        for_node->for_stmt.increment_expr = increment;
    }
    else
    {
        TRACE(ParseForStatement_range);
        Ast_Node *range_expr = ParseVarDeclExpr(ctx);
        if (!range_expr) range_expr = ParseExpression(ctx);
        if (!range_expr)
        {
            Error(ctx, "Expecting range expression after for");
        }
        for_node->for_stmt.range_expr = range_expr;
    }
    Ast_Node *loop_stmt = ParseStatement(ctx);
    if (!loop_stmt)
    {
        Error(ctx, "Expecting statement after for");
    }
    for_node->for_stmt.loop_stmt = loop_stmt;
    return for_node;
}

static Ast_Node* ParseReturnStatement(Parser_Context *ctx)
{
    TRACE(ParseReturnStatement);
    const Token *return_tok = Accept(ctx, TOK_Return);
    if (!return_tok) return nullptr;

    Ast_Node *return_node = PushNode(ctx, AST_ReturnStmt, return_tok);
    Ast_Node *expr = ParseExpression(ctx);
    return_node->return_stmt.expression = expr;
    ExpectAfterLast(ctx, TOK_Semicolon);
    return return_node;
}

static Ast_Node* ParseType(Parser_Context *ctx);

static Ast_Node* ParseVarDeclExpr(Parser_Context *ctx)
{
    const Token *ident_tok = GetCurrentToken(ctx);
    if (ident_tok->type != TOK_Identifier)
        return nullptr;

    const Token *peek = PeekNextToken(ctx);
    if (peek->type != TOK_Colon && peek->type != TOK_ColonEq)
        return nullptr;

    Accept(ctx, TOK_Identifier);

    Ast_Node *var_decl = PushNode(ctx, AST_VariableDecl, ident_tok);
    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
    var_decl->variable_decl.name = name;

    Ast_Node *type = nullptr;
    Ast_Node *init = nullptr;
    if (Accept(ctx, TOK_Colon))
    {
        type = ParseType(ctx);
        if (!type)
        {
            Error(ctx, "Expecting type for variable");
        }

        if (Accept(ctx, TOK_Eq))
        {
            init = ParseExpression(ctx);
            if (!init)
            {
                Error(ctx, "Expecting initializing expression for variable");
            }
        }
    }
    else
    {
        Expect(ctx, TOK_ColonEq);
        init = ParseExpression(ctx);
        if (!init)
        {
            Error(ctx, "Expecting initializing expression for variable");
        }
    }
    var_decl->variable_decl.type = type;
    var_decl->variable_decl.init = init;
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

static Ast_Node* ParseStatement(Parser_Context *ctx)
{
    TRACE(ParseStatement);
    Ast_Node *stmt = ParseBlockStatement(ctx);
    if (!stmt) stmt = ParseIfStatement(ctx);
    if (!stmt) stmt = ParseForStatement(ctx);
    if (!stmt) stmt = ParseReturnStatement(ctx);
    if (!stmt) stmt = ParseVarDeclStatement(ctx);
    if (!stmt)
    {
        Ast_Node *expression = ParseExpression(ctx);
        if (expression)
        {
            ExpectAfterLast(ctx, TOK_Semicolon);
            //Expect(ctx, TOK_Semicolon);
        }
        stmt = expression;
    }
    return stmt;
}

static Ast_Node* ParseType(Parser_Context *ctx)
{
    TRACE(ParseType);
    Ast_Node *type_node = nullptr;
    const Token *token = GetCurrentToken(ctx);
    switch (token->type)
    {
        case TOK_OpenParent:
            {
                GetNextToken(ctx);
                type_node = ParseType(ctx);
                ExpectAfterLast(ctx, TOK_CloseParent);
            } break;

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
                type_node = PushNode(ctx, AST_Type_Plain, token);
                Name name = PushName(&ctx->ast->arena,
                        token->value, token->value_end);
                type_node->type_node.plain.name = name;
            } break;

        default: break;
    }
    if (!type_node)
        return nullptr;

    while (ContinueParsing(ctx))
    {
        const Token *token = Accept(ctx, TOK_Star);
        if (token)
        {
            Ast_Node *pointer_node = PushNode(ctx, AST_Type_Pointer, token);
            s64 indirection = 0;
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
            Ast_Node *array_node = PushNode(ctx, AST_Type_Array, token);
            ExpectAfterLast(ctx, TOK_CloseBracket);
            s64 arrays = 0;
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

static void ParseParameters(Parser_Context *ctx, Ast_Node *func_def)
{
    TRACE(ParseParameters);
    ASSERT(func_def->type == AST_FunctionDef);
    do
    {
        const Token *token = GetCurrentToken(ctx);
        if (token->type == TOK_Identifier)
        {
            GetNextToken(ctx);
            Ast_Node *param_node = PushNode(ctx, AST_Parameter, token);
            Name name = PushName(&ctx->ast->arena,
                    token->value, token->value_end);
            param_node->parameter.name = name;

            Expect(ctx, TOK_Colon);
            Ast_Node *type_node = ParseType(ctx);
            param_node->parameter.type = type_node;

            PushNodeList(&func_def->function.parameters, param_node);
        }
        else if (token->type == TOK_CloseParent)
        {
            break;
        }
        else
        {
            Error(ctx, token, "Expecting parameter name");
        }
    } while (Accept(ctx, TOK_Comma) && ContinueParsing(ctx));
}

static Ast_Node* ParseFunction(Parser_Context *ctx, const Token *ident_tok)
{
    TRACE(ParseFunction);
    if (!Accept(ctx, TOK_OpenParent)) return nullptr;

    Ast_Node *func_def = PushNode(ctx, AST_FunctionDef, ident_tok);
    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
    func_def->function.name = name;

    ParseParameters(ctx, func_def);
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
        func_def->function.return_type = return_type;
        if (!return_type)
        {
            Error(ctx, "Expecting function return type");
        }
    }
    Ast_Node *body = ParseBlockStatement(ctx);
    if (!body)
    {
        Error(ctx, "Expecting function body");
    }
    func_def->function.body = body;
    return func_def;
}

static Ast_Node* ParseStructMember(Parser_Context *ctx)
{
    const Token *ident_tok = Accept(ctx, TOK_Identifier);
    if (!ident_tok) return nullptr;

    ExpectAfterLast(ctx, TOK_Colon);

    Ast_Node *member = PushNode(ctx, AST_StructMember, ident_tok);
    Ast_Node *type = ParseType(ctx);

    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
    member->struct_member.name = name;
    member->struct_member.type = type;

    ExpectAfterLast(ctx, TOK_Semicolon);

    return member;
}

static Ast_Node* ParseStruct(Parser_Context *ctx, const Token *ident_tok)
{
    const Token *struct_tok = Accept(ctx, TOK_Struct);
    if (!struct_tok) return nullptr;

    Ast_Node *struct_def = PushNode(ctx, AST_StructDef, ident_tok);
    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
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

    Ast_Node *import_node = PushNode(ctx, AST_Import, import_tok);
    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
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

    Ast_Node *import_node = PushNode(ctx, AST_Import, import_tok);
    String mod_name_str = ConvertString(ctx,
            module_name_tok->value, module_name_tok->value_end);
    import_node->import.name = { };
    import_node->import.module_name = mod_name_str;

    return import_node;
}

static Ast_Node* ParseForeignFunction(Parser_Context *ctx, const Token *ident_tok)
{
    TRACE(ParseForeignFunction);
    if (!Accept(ctx, TOK_OpenParent)) return nullptr;

    Ast_Node *func_def = PushNode(ctx, AST_FunctionDef, ident_tok);
    Name name = PushName(&ctx->ast->arena,
            ident_tok->value, ident_tok->value_end);
    func_def->function.name = name;

    ParseParameters(ctx, func_def);
    ExpectAfterLast(ctx, TOK_CloseParent);
    // TODO(henrik): Should we make the return type of a foreign function
    // mandatory?
    if (Accept(ctx, TOK_Colon))
    {
        // NOTE(henrik): The return type node can be nullptr. This means
        // that the type will be inferred later. But, as this leads to
        // ambiguous syntax for inferred return type and no return type, the
        // syntax should be changed. E.g. maybe
        //   func_name :: (...) : *
        // will have inferred return type.
        Ast_Node *return_type = ParseType(ctx);
        func_def->function.return_type = return_type;
        if (!return_type)
        {
            Error(ctx, "Expecting function return type");
        }
    }
    Expect(ctx, TOK_Semicolon);
    return func_def;
}

static Ast_Node* ParseForeignStmt(Parser_Context *ctx)
{
    TRACE(ParseForeignStmt);

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

    Ast_Node *foreign_block = PushNode(ctx, AST_ForeignBlock, foreign_tok);

    Expect(ctx, TOK_OpenBlock);
    while (ContinueParsing(ctx))
    {
        Ast_Node *stmt = ParseForeignStmt(ctx);
        if (!stmt)
        break;

        PushNodeList(&foreign_block->node_list, stmt);
    }
    Expect(ctx, TOK_CloseBlock);

    return foreign_block;
}

static Ast_Node* ParseTopLevelNamedStmt(Parser_Context *ctx)
{
    TRACE(ParseTopLevelIdentifier);

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
    if (!stmt) stmt = ParseFunction(ctx, ident_tok);
    return stmt;
}

// Parses the following top level statements:
// import "module_name";
// name :: import "module_name";
// func_name :: (...) { ... }
// struct_name :: struct { ... }    (not implemented yet)
// enum_name :: enum { ... }        (not implemented yet)
static Ast_Node* ParseTopLevelStmt(Parser_Context *ctx)
{
    TRACE(ParseTopLevelStmt);
    Ast_Node *stmt = ParseGlobalImport(ctx);
    if (!stmt) stmt = ParseForeignBlock(ctx);
    if (!stmt) stmt = ParseVarDeclStatement(ctx);
    if (!stmt) stmt = ParseTopLevelNamedStmt(ctx);
    return stmt;
}

b32 Parse(Parser_Context *ctx)
{
    ASSERT(ctx && ctx->ast);
    ctx->ast->root = PushNode(ctx, AST_TopLevel, GetCurrentToken(ctx));
    Ast_Node *root = ctx->ast->root;

    while (ContinueParsing(ctx))
    {
        Ast_Node *stmt = ParseTopLevelStmt(ctx);
        if (stmt)
        {
            PushNodeList(&root->node_list, stmt);
        }
        else if (ContinueParsing(ctx))
        {
            //Error(ctx, "Unexpected token");
            ErrorInvalidToken(ctx, GetCurrentToken(ctx));
            GetNextToken(ctx);
        }
    }
    return HasError(ctx->comp_ctx);
}

} // hplang
