
#include "parser.h"
#include "error.h"
#include "assert.h"

namespace hplang
{

Parser_Context NewParserContext(
        Token_List tokens,
        Error_Context *error_ctx,
        Compiler_Options *options)
{
    Parser_Context ctx = { };
    ctx.tokens = tokens;
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

static void Error(Parser_Context *ctx, File_Location file_loc,
            const char *message, const Token *token)
{
    AddError(ctx->error_ctx, file_loc);
    if (!token)
    {
        PrintFileLocation(ctx->error_ctx->file, file_loc);
        fprintf(ctx->error_ctx->file, ": %s\n", message);
    }
    else
    {
        PrintFileLocation(ctx->error_ctx->file, file_loc);
        fprintf(ctx->error_ctx->file, ": %s '", message);
        PrintTokenValue(ctx->error_ctx->file, token);
        fprintf(ctx->error_ctx->file, "'\n");
    }
}

static void ErrorExpected(Parser_Context *ctx, File_Location file_loc,
        const char *expected_token, const Token *token)
{
    AddError(ctx->error_ctx, file_loc);
    if (!token)
    {
        PrintFileLocation(ctx->error_ctx->file, file_loc);
        fprintf(ctx->error_ctx->file, ": Expected %s\n", expected_token);
    }
    else
    {
        PrintFileLocation(ctx->error_ctx->file, file_loc);
        fprintf(ctx->error_ctx->file, ": Expected %s, instead got ", expected_token);
        PrintTokenValue(ctx->error_ctx->file, token);
        fprintf(ctx->error_ctx->file, "\n");
    }
}

static void Error_UnexpectedEOF(Parser_Context *ctx, const Token *token)
{
    Error(ctx, token->file_loc, "Unexpected end of file after", token);
}


static Token eof_token = { TOK_EOF };

static const Token* GetNextToken(Parser_Context *ctx)
{
    //const Token *token = GetNextToken(ctx->tokens);
    //return token ? token : &eof_token;
    if (ctx->current_token < ctx->tokens.count)
        return ctx->tokens.begin + ctx->current_token++;
    return &eof_token;
}

static const Token* GetCurrentToken(Parser_Context *ctx)
{
    if (ctx->current_token < ctx->tokens.count)
        return ctx->tokens.begin + ctx->current_token;
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
        ErrorExpected(ctx, token->file_loc,
                TokenTypeToString(token_type), token);
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

    const Token *token = GetNextToken(ctx);
    while (ContinueParsing(ctx))
    {
        switch (token->type)
        {
        case TOK_Import:
            ParseImport(ctx, nullptr, ctx->ast_root);
            break;
        case TOK_Identifier:
            ParseTopLevelIdentifier(ctx, token, ctx->ast_root);
            break;
        case TOK_EOF:
            break;
        default:
            Error(ctx, token->file_loc, "Unexpected token", token);
            fprintf(ctx->error_ctx->file, "TOK(%s)\n", TokenTypeToString(token->type));
        }
        if (token->type == TOK_EOF)
            break;
        token = GetNextToken(ctx);
    }
}

void ParseImport(Parser_Context *ctx, const Token *ident_tok, Ast_Node *root)
{
    const Token *prev_token = GetCurrentToken(ctx);
    const Token *token = GetNextToken(ctx);
    switch (token->type)
    {
        case TOK_StringLit:
            {
                Ast_Node *import_node = PushNode(ctx, AST_Import,
                        ident_tok ? ident_tok : prev_token);
                import_node->import.name = ident_tok;
                import_node->import.module_name = token;
                PushNodeList(&root->node_list, import_node);
            }
            break;

        case TOK_EOF:
            Error_UnexpectedEOF(ctx, prev_token);
            return;
        default:
            Error(ctx, token->file_loc, "Expecting string literal, not", token);
    }
    Expect(ctx, TOK_Semicolon);
}

Ast_Node* ParseStmtBlock(Parser_Context *ctx)
{
    PushNode(ctx, AST_StmtBlock, GetCurrentToken(ctx));
    Expect(ctx, TOK_OpenBlock);
    do
    {
        if (Accept(ctx, TOK_CloseBlock)) break;
        else
        {
            break;
        }
    } while (true);
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
                Ast_Node *type_node = ParseType(ctx);
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
                Ast_Node *type_node = PushNode(ctx, AST_Type_Plain, token);
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
            while (token->type == TOK_Star)
            {
                pointer_node->type_node.pointer.indirection++;
                token = GetNextToken(ctx);
            }
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
            Error(ctx, token->file_loc, "Expected parameter name, got", token);
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
