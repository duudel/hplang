
#include "parser.h"
#include "error.h"
#include "assert.h"

namespace hplang
{

Parser_Context NewParserContext(
        Token_List tokens,
        Open_File *open_file,
        Error_Context *error_ctx,
        Compiler_Options *options)
{
    Parser_Context ctx = { };
    ctx.tokens = tokens;
    ctx.open_file = open_file;
    ctx.error_ctx = error_ctx;
    ctx.options = options;
    return ctx;
}

void FreeParserContext(Parser_Context *ctx)
{
    FreeMemoryArena(&ctx->arena);
}

static b32 ContinueParsing(Parser_Context *ctx)
{
    return ctx->error_ctx->error_count < ctx->options->max_error_count;
}

static void PrintFileLine(Error_Context *error_ctx,
        Open_File *open_file,
        File_Location file_loc)
{
    const char *file_start = (const char*)open_file->contents.ptr;
    ASSERT(file_start != nullptr);
    ASSERT(file_loc.line_offset < open_file->contents.size);

    const char *line_start = file_start + file_loc.line_offset;
    s64 line_len = 0;

    while (line_len < open_file->contents.size)
    {
        char c = line_start[line_len];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f')
            break;
        line_len++;
    }

    fwrite(line_start, 1, line_len, error_ctx->file);
    fprintf(error_ctx->file, "\n");
}

static void PrintFileLocArrow(Error_Context *ctx, File_Location file_loc)
{
    const char dashes[81] = "--------------------------------------------------------------------------------";
    if (file_loc.column > 0 && file_loc.column < 81 - 1)
    {
        fwrite(dashes, 1, file_loc.column - 1, ctx->file);
        fprintf(ctx->file, "^\n");
    }
}

static void PrintSourceLineAndArrow(Parser_Context *ctx, File_Location file_loc)
{
    if (ctx->error_ctx->error_count < 3)
    {
        fprintf(ctx->error_ctx->file, "\n");
        PrintFileLine(ctx->error_ctx, ctx->open_file, file_loc);
        PrintFileLocArrow(ctx->error_ctx, file_loc);
        fprintf(ctx->error_ctx->file, "\n");
    }
}

static void Error(Parser_Context *ctx, const Token *token, const char *message)
{
    AddError(ctx->error_ctx, token->file_loc);
    PrintFileLocation(ctx->error_ctx->file, token->file_loc);
    fprintf(ctx->error_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx, token->file_loc);
}

static void ErrorExpected(Parser_Context *ctx,
        const Token *token,
        const char *expected_token)
{
    AddError(ctx->error_ctx, token->file_loc);
    PrintFileLocation(ctx->error_ctx->file, token->file_loc);
    fprintf(ctx->error_ctx->file, "Expecting %s\n", expected_token);
    PrintSourceLineAndArrow(ctx, token->file_loc);
}

static void Error_UnexpectedEOF(Parser_Context *ctx, const Token *token)
{
    Error(ctx, token, "Unexpected end of file");
}


static Token eof_token = { TOK_EOF };

static const Token* GetCurrentToken(Parser_Context *ctx)
{
    if (ctx->current_token < ctx->tokens.count)
        return ctx->tokens.begin + ctx->current_token;
    return &eof_token;
}

static const Token* GetNextToken(Parser_Context *ctx)
{
    //fprintf(stderr, "  : ");
    //PrintTokenValue(stderr, GetCurrentToken(ctx));
    //fprintf(stderr, "\n");
    if (ctx->current_token < ctx->tokens.count)
        return ctx->tokens.begin + ++ctx->current_token;
    return &eof_token;
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
    if (token)
        return token;
    token = GetCurrentToken(ctx);
    ErrorExpected(ctx, token, TokenTypeToString(token_type));
    return nullptr;
}

static Ast_Node* PushNode(Parser_Context *ctx, Ast_Type type, const Token *token)
{
    Ast_Node *node = PushStruct<Ast_Node>(&ctx->arena);
    *node = { };
    node->type = type;
    return node;
}

static void ParseImport(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node);
static void ParseTopLevelIdentifier(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node);

b32 Parse(Parser_Context *ctx)
{
    ctx->ast_root = PushNode(ctx, AST_TopLevel, nullptr);

    const Token *token = GetCurrentToken(ctx);
    while (ContinueParsing(ctx))
    {
        if (token->type == TOK_EOF)
            break;
        switch (token->type)
        {
        case TOK_Import:
            GetNextToken(ctx);
            ParseImport(ctx, nullptr, ctx->ast_root);
            break;
        case TOK_Identifier:
            GetNextToken(ctx);
            ParseTopLevelIdentifier(ctx, token, ctx->ast_root);
            break;
        default:
            Error(ctx, token, "Unexpected token");
            GetNextToken(ctx);
        }
        token = GetCurrentToken(ctx);
    }
    return ctx->error_ctx->error_count != 0;
}

static void ParseImport(Parser_Context *ctx, const Token *ident_tok, Ast_Node *root)
{
    const Token *token = GetCurrentToken(ctx);
    switch (token->type)
    {
        case TOK_StringLit:
            {
                Ast_Node *import_node = PushNode(ctx, AST_Import,
                        ident_tok ? ident_tok : token);
                import_node->import.name = ident_tok;
                import_node->import.module_name = token;
                PushNodeList(&root->node_list, import_node);
            }
            break;

        case TOK_EOF:
            Error_UnexpectedEOF(ctx, token);
            return;
        default:
            Error(ctx, token, "Expecting string literal");
    }
    GetNextToken(ctx);
    Expect(ctx, TOK_Semicolon);
}

static Ast_Node* ParseExpression(Parser_Context *ctx);

static s64 ParseInt(const char *s, const char *end)
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

static f64 ParseFloat(const char *s, const char *end)
{
    // TODO(henrik): Implement float parsing with c-standard lib
    return 0.0;
}

static String ParseString(Memory_Arena *arena, const char *s, const char *end)
{
    // NOTE(henrik): Assumes that the resulting string can only be as long or
    // shorter than the original string. This is due to escaping sequences
    // that never resulting more characters than is used to represent them.
    // We could also use the fact that no one else is going to allocate from
    // the same arena, as we construct the string value, to allocate each
    // character as we advance the string to make the used memory match the
    // length of the string (so no extra memory is reserved).
    String result = PushString(arena, s, end - s);
    s64 i = 0;
    while (s != end)
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
                        case 't': c = '\t'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                    }
                }
                else
                {
                    // error
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
    const Token *token = Accept(ctx, TOK_IntegerLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_IntLiteral, token);
        literal->expression.int_literal.value = ParseInt(token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_FloatLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_FloatLiteral, token);
        literal->expression.float_literal.value = ParseFloat(token->value, token->value_end);
        return literal;
    }
    token = Accept(ctx, TOK_StringLit);
    if (token)
    {
        Ast_Node *literal = PushNode(ctx, AST_StringLiteral, token);
        literal->expression.string_literal.value = ParseString(&ctx->arena, token->value, token->value_end);
        return literal;
    }
}

static Ast_Node* ParsePrefixOperator(Parser_Context *ctx)
{
    const Token *op_token = Accept(ctx, TOK_Plus);
    if (!op_token) op_token = Accept(ctx, TOK_Minus);
    if (!op_token) op_token = Accept(ctx, TOK_Tilde);
    if (!op_token) op_token = Accept(ctx, TOK_Bang);

    if (!op_token) return nullptr;

    Ast_Node *pre_op = PushNode(ctx, AST_UnaryExpr, op_token);

    Ast_Unary_Op op;
    switch (op_token->type)
    {
        case TOK_Plus:  op = AST_OP_Positive; break;
        case TOK_Minus: op = AST_OP_Negative; break;
        case TOK_Tilde: op = AST_OP_Complement; break;
        case TOK_Bang:  op = AST_OP_Not; break;

        // TODO(henrik): Add parsing of & (address-of) and @ (deference) operators
    }
    pre_op->expression.unary_expr.op = op;
    pre_op->expression.unary_expr.expr = nullptr;
    return pre_op;
}

static Ast_Node* ParseFunctionArgs(Parser_Context *ctx)
{
    Ast_Node *args = PushNode(ctx, AST_FunctionCallArgs, GetCurrentToken(ctx));

    Ast_Node *arg = ParseExpression(ctx);
    if (arg)
    {
        PushNodeList(&args->node_list, arg);
        while (Accept(ctx, TOK_Comma))
        {
            Ast_Node *arg = ParseExpression(ctx);
            if (!arg)
            {
                Error(ctx, GetCurrentToken(ctx),
                        "Expecting function argument after comma");
                break;
            }
            PushNodeList(&args->node_list, arg);
        }
    }
    return args;
}

static Ast_Node* ParseFactorExpr(Parser_Context *ctx)
{
    Ast_Node *pre_op = ParsePrefixOperator(ctx);

    Ast_Node *factor = ParseLiteralExpr(ctx);

    if (!factor)
    {
        const Token *identifier_tok = Accept(ctx, TOK_Identifier);
        if (identifier_tok)
        {
            // NOTE(henrik): Only accept function call for identifiers.
            // Is there any benefit to accept same syntax for other types of
            // factors? Maybe even for error reporting reasonsr?
            if (Accept(ctx, TOK_OpenParent))
            {
                factor = PushNode(ctx, AST_FunctionCall, identifier_tok);
                Ast_Node *args = ParseFunctionArgs(ctx);
                factor->expression.function_call.args = args;
                Expect(ctx, TOK_CloseParent);
            }
            else
            {
                factor = PushNode(ctx, AST_VariableRef, identifier_tok);
            }
        }
    }

    if (!factor && Accept(ctx, TOK_OpenParent))
    {
        factor = ParseExpression(ctx);
        if (!factor)
        {
            Error(ctx, GetCurrentToken(ctx),
                    "Expecting expression after left parenthesis");
        }
        Expect(ctx, TOK_CloseParent);
    }

    if (!factor)
    {
        if (pre_op)
        {
            Error(ctx, GetCurrentToken(ctx),
                    "Expecting operand for unary prefix operator");
        }
        return nullptr;
    }

    // TODO(henrik): post ops

    if (pre_op)
    {
        pre_op->expression.unary_expr.expr = factor;
        factor = pre_op;
    }

    return factor;
}

static Ast_Node* ParseMultDivExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseFactorExpr(ctx);
    if (!expr) return nullptr;

    while (true)
    {
        const Token *op_token = Accept(ctx, TOK_Star);
        if (!op_token) op_token = Accept(ctx, TOK_Slash);
        if (!op_token) break;

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Star:  op = AST_OP_Multiply; break;
            case TOK_Slash: op = AST_OP_Divide; break;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseFactorExpr(ctx);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseAddSubExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseMultDivExpr(ctx);
    if (!expr) return nullptr;

    while (true)
    {
        const Token *op_token = Accept(ctx, TOK_Plus);
        if (!op_token) op_token = Accept(ctx, TOK_Minus);
        if (!op_token) break;

        Ast_Binary_Op op;
        switch (op_token->type)
        {
            case TOK_Plus:  op = AST_OP_Add; break;
            case TOK_Minus: op = AST_OP_Subtract; break;
        }

        Ast_Node *bin_expr = PushNode(ctx, AST_BinaryExpr, op_token);
        bin_expr->expression.binary_expr.op = op;
        bin_expr->expression.binary_expr.left = expr;
        bin_expr->expression.binary_expr.right = ParseMultDivExpr(ctx);

        expr = bin_expr;
    }
    return expr;
}

static Ast_Node* ParseLogicalExpr(Parser_Context *ctx)
{
    //Ast_Node *expr = ParseShiftExpr(ctx);
    Ast_Node *expr = ParseAddSubExpr(ctx);
    return expr;
}

static Ast_Node* ParseComparisonExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseLogicalExpr(ctx);
    return expr;
}

static Ast_Node* ParseAssignmentExpr(Parser_Context *ctx)
{
    Ast_Node *expr = ParseComparisonExpr(ctx);
    return expr;
}

static Ast_Node* ParseExpression(Parser_Context *ctx)
{
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
    // z = x = y / 7 * 2
    //   =
    //  / \
    // z   =
    //    / \
    //   x   /
    //      / \
    //     y   *
    //        / \
    //       7   2
    Ast_Node *expr = ParseAssignmentExpr(ctx);
    return expr;
}

static Ast_Node* ParseStmtBlock(Parser_Context *ctx);
static Ast_Node* ParseIfStatement(Parser_Context *ctx, const Token *if_tok);

static Ast_Node* ParseStatement(Parser_Context *ctx)
{
    const Token *token = GetCurrentToken(ctx);
    if (token->type == TOK_OpenBlock)
    {
        return ParseStmtBlock(ctx);
    }
    if (Accept(ctx, TOK_If))
    {
        return ParseIfStatement(ctx, token);
    }
    Ast_Node *expression = ParseExpression(ctx);
    if (expression)
    {
        Expect(ctx, TOK_Semicolon);
    }
    else
    {
        Error(ctx, token, "Expecting statement");
        GetNextToken(ctx);
    }
    return nullptr;
}

static Ast_Node* ParseStmtBlock(Parser_Context *ctx)
{
    Ast_Node *block_node = PushNode(ctx, AST_StmtBlock, GetCurrentToken(ctx));
    Expect(ctx, TOK_OpenBlock);
    do
    {
        if (Accept(ctx, TOK_CloseBlock))
            break;
        Ast_Node *stmt_node = ParseStatement(ctx);
        if (stmt_node)
        {
            PushNodeList(&block_node->node_list, stmt_node);
        }
    } while (ContinueParsing(ctx));
    return block_node;
}

static Ast_Node* ParseIfStatement(Parser_Context *ctx, const Token *if_tok)
{
    Ast_Node *if_node = PushNode(ctx, AST_IfStmt, if_tok);
    Ast_Node *expr = ParseExpression(ctx);
    if (!expr)
    {
        Error(ctx, GetCurrentToken(ctx),
                "Expecting condition expression for if statement");
        GetNextToken(ctx);
    }
    Ast_Node *true_stmt = ParseStatement(ctx);
    Ast_Node *false_stmt = nullptr;
    if (Accept(ctx, TOK_Else))
    {
        false_stmt = ParseStatement(ctx);
    }
    if_node->if_stmt.condition_expr = expr;
    if_node->if_stmt.true_stmt = true_stmt;
    if_node->if_stmt.false_stmt = false_stmt;
    return if_node;
}

static Ast_Node* ParseType(Parser_Context *ctx)
{
    const Token *token = GetCurrentToken(ctx);
    Ast_Node *type_node = nullptr;
    switch (token->type)
    {
        case TOK_OpenParent:
            {
                GetNextToken(ctx);
                type_node = ParseType(ctx);
                Expect(ctx, TOK_CloseParent);
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
            {
                GetNextToken(ctx);
                type_node = PushNode(ctx, AST_Type_Plain, token);
            } break;
    }
    if (!type_node)
        return nullptr;
    while (true)
    {
        const Token *token = GetCurrentToken(ctx);
        if (token->type == TOK_Star)
        {
            Ast_Node *pointer_node = PushNode(ctx, AST_Type_Pointer, token);
            s64 indirection = 0;
            while (token->type == TOK_Star)
            {
                indirection++;
                token = GetNextToken(ctx);
            }
            pointer_node->type_node.pointer.indirection = indirection;
            pointer_node->type_node.pointer.base_type = type_node;
            type_node = pointer_node;
        }
        else if (token->type == TOK_OpenBracket)
        {
            Ast_Node *array_node = PushNode(ctx, AST_Type_Array, token);
            GetNextToken(ctx);
            Expect(ctx, TOK_CloseBracket);
            while (token->type == TOK_Star)
            {
                array_node->type_node.array.array++;

                GetNextToken(ctx);
                Expect(ctx, TOK_CloseBracket);
                token = GetNextToken(ctx);
            }
            array_node->type_node.array.base_type = type_node;
            type_node = array_node;
        }
        else
        {
            break;
        }
    }
    return type_node;
}

static void ParseParameters(Parser_Context *ctx, Ast_Node *func_def)
{
    ASSERT(func_def->type == AST_FunctionDef);
    do
    {
        const Token *token = GetCurrentToken(ctx);
        if (token->type == TOK_Identifier)
        {
            GetNextToken(ctx);
            Ast_Node *param_node = PushNode(ctx, AST_Parameter, token);
            param_node->parameter.name = token;

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

static void ParseFunction(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node)
{
    Ast_Node *func_def = PushNode(ctx, AST_FunctionDef, ident_tok);
    ParseParameters(ctx, func_def);
    Expect(ctx, TOK_CloseParent);
    if (Accept(ctx, TOK_Colon))
    {
        // NOTE(henrik): the return type node can be nullptr
        Ast_Node *return_type = ParseType(ctx);
        func_def->function.return_type = return_type;
    }
    Ast_Node *block = ParseStmtBlock(ctx);
    if (block)
    {
        func_def->function.body = block;
    }
}

static void ParseTopLevelIdentifier(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node)
{
    if (Accept(ctx, TOK_ColonColon))
    {
        if (Accept(ctx, TOK_Import))
        {
            ParseImport(ctx, ident_tok, node);
        }
        else if (Accept(ctx, TOK_OpenParent))
        {
            ParseFunction(ctx, ident_tok, node);
        }
    }
}

} // hplang
