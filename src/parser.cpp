
#include "parser.h"
#include "error.h"

namespace hplang
{

Parser_Context NewParserContext(Token_Arena *tokens)
{
    Parser_Context ctx = { };
    ctx.tokens = tokens;
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
        fprintf(ctx->error_ctx->file, "' (%i)\n", token->value[0]);
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
    const Token *token = GetNextToken(ctx->tokens);
    return token ? token : &eof_token;
}

static const Token* GetCurrentToken(Parser_Context *ctx)
{
    const Token *token = ctx->tokens->current;
    return token ? token : &eof_token;
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
        }
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

void ParseTopLevelIdentifier(Parser_Context *ctx, const Token *ident, Ast_Node *node)
{
    if (Accept(ctx, TOK_ColonColon))
    {
        const Token *token = GetCurrentToken(ctx);
        switch (token->type)
        {
        case TOK_Import:
            ParseImport(ctx, ident, node);
            break;
        // Other cases
        }
    }
}

} // hplang
