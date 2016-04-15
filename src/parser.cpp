
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

//static void Error(Parser_Context *ctx, File_Location file_loc,
//            const char *message, const Token *token)
//{
//    AddError(ctx->error_ctx, file_loc);
//    if (!token)
//    {
//        PrintFileLocation(ctx->error_ctx->file, file_loc);
//        fprintf(ctx->error_ctx->file, ": %s\n", message);
//    }
//    else
//    {
//        PrintFileLocation(ctx->error_ctx->file, file_loc);
//        fprintf(ctx->error_ctx->file, ": %s '", message);
//        PrintTokenValue(ctx->error_ctx->file, token);
//        fprintf(ctx->error_ctx->file, "'\n");
//    }
//}

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

static b32 Accept(Parser_Context *ctx, Token_Type token_type)
{
    const Token *token = GetCurrentToken(ctx);
    if (token->type == token_type)
    {
        GetNextToken(ctx);
        return true;
    }
    return false;
}

static b32 Expect(Parser_Context *ctx, Token_Type token_type)
{
    if (!Accept(ctx, token_type))
    {
        const Token *token = GetCurrentToken(ctx);
        ErrorExpected(ctx, token, TokenTypeToString(token_type));
        return false;
    }
    return true;
}

static Ast_Node* PushNode(Parser_Context *ctx, Ast_Type type, const Token *token)
{
    Ast_Node *node = PushStruct<Ast_Node>(&ctx->arena);
    *node = { };
    node->type = type;
    return node;
}

void ParseImport(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node);
void ParseTopLevelIdentifier(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node);

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

void ParseImport(Parser_Context *ctx, const Token *ident_tok, Ast_Node *root)
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

Ast_Node* ParseExpression(Parser_Context *ctx)
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
    //Ast_Node *expr = ParseAssignmentExpr(ctx);
    //return expr;
    return nullptr;
}

Ast_Node* ParseStmtBlock(Parser_Context *ctx);
Ast_Node* ParseIfStatement(Parser_Context *ctx, const Token *if_tok);

Ast_Node* ParseStatement(Parser_Context *ctx)
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
    else
    {
        Error(ctx, token, "Expecting statement");
        GetNextToken(ctx);
    }
    return nullptr;
}

Ast_Node* ParseStmtBlock(Parser_Context *ctx)
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

Ast_Node* ParseIfStatement(Parser_Context *ctx, const Token *if_tok)
{
    Ast_Node *if_node = PushNode(ctx, AST_IfStmt, if_tok);
    Ast_Node *expr = ParseExpression(ctx);
    if (!expr)
    {
        Error(ctx, GetCurrentToken(ctx), "Expecting condition expression for if statement");
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

Ast_Node* ParseType(Parser_Context *ctx)
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

void ParseParameters(Parser_Context *ctx, Ast_Node *func_def)
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

void ParseFunction(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node)
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

void ParseTopLevelIdentifier(Parser_Context *ctx, const Token *ident_tok, Ast_Node *node)
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
