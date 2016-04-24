
#include "semantic_check.h"
#include "ast_types.h"
#include "compiler.h"
#include "error.h"
#include "compiler_options.h"
#include "assert.h"

namespace hplang
{

Sem_Check_Context NewSemanticCheckContext(
        Ast *ast,
        Open_File *open_file,
        Compiler_Context *comp_ctx)
{
    Sem_Check_Context ctx = { };
    ctx.ast = ast;
    ctx.open_file = open_file;
    ctx.comp_ctx = comp_ctx;
    return ctx;
}

void UnlinkAst(Sem_Check_Context *ctx)
{
    ctx->ast = nullptr;
}

void FreeSemanticCheckContext(Sem_Check_Context *ctx)
{
    FreeMemoryArena(&ctx->temp_arena);
    if (ctx->ast)
    {
        FreeAst(ctx->ast);
        ctx->ast = nullptr;
    }
}

// Semantic check
// --------------

static b32 ContinueChecking(Sem_Check_Context *ctx)
{
    return ContinueCompiling(ctx->comp_ctx);
}

void PrintString(FILE *file, String str)
{
    fwrite(str.data, 1, str.size, file);
}

void ErrorImport(Sem_Check_Context *ctx, Ast_Node *node, String filename)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->token->file_loc);
    PrintFileLocation(err_ctx->file, node->token->file_loc);
    fprintf(err_ctx->file, "Could not open file '");
    PrintString(err_ctx->file, filename);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->token->file_loc);
}

void Error(Sem_Check_Context *ctx, Ast_Node *node, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->token->file_loc);
    PrintFileLocation(err_ctx->file, node->token->file_loc);
    fprintf(err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->token->file_loc);
}

void CheckImport(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->import.module_name.data);
    String module_filename = { };
    Open_File *open_file = OpenModule(ctx->comp_ctx,
            ctx->open_file, node->import.module_name, &module_filename);
    if (!open_file)
    {
        ErrorImport(ctx, node, module_filename);
        return;
    }
    CompileModule(ctx->comp_ctx, open_file, module_filename);
}

void CheckFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function.name.str.data);
}

struct Type;
struct StructType;

Type* CheckType(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_Type_Plain:
            Error(ctx, node, "Plain type not implemented yet");
            break;
        case AST_Type_Pointer:
            Error(ctx, node, "Pointer type not implemented yet");
            CheckType(ctx, node->type_node.pointer.base_type);
            break;
        case AST_Type_Array:
            Error(ctx, node, "Array type not implemented yet");
            CheckType(ctx, node->type_node.array.base_type);
            break;
        default:
            INVALID_CODE_PATH;
    }
    return nullptr;
}

void CheckStructMember(Sem_Check_Context *ctx, Ast_Node *node, StructType *s)
{
    ASSERT(node);
    switch (node->type)
    {
        case AST_StructMember:
            {
                Type *type = CheckType(ctx, node->struct_member.type);
                (void)type;
                //StructMember *member = PushStructMember(ctx->env,
                //        node->struct_member.name, type);
            } break;
        default:
            INVALID_CODE_PATH;
    }
}

void CheckStruct(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->struct_def.name.str.data);
    for (s64 i = 0; i < node->struct_def.members.count; i++)
    {
        CheckStructMember(ctx, node->struct_def.members.nodes[i], nullptr);
    }
}

void CheckVariableDecl(Sem_Check_Context *ctx, Ast_Node *node)
{
    CheckType(ctx, node->variable_decl.type);
}

void CheckForeignFunction(Sem_Check_Context *ctx, Ast_Node *node)
{

}

void CheckForeignBlockStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_FunctionDef:   CheckForeignFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        default:
            INVALID_CODE_PATH;
    }
}

void CheckForeignBlock(Sem_Check_Context *ctx, Ast_Node *node)
{
    for (s64 i = 0; i < node->node_list.count; i++)
    {
        Ast_Node *stmt = node->node_list.nodes[i];
        CheckForeignBlockStmt(ctx, stmt);
    }
}

void CheckTopLevelStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_Import:        CheckImport(ctx, node); break;
        case AST_ForeignBlock:  CheckForeignBlock(ctx, node); break;
        case AST_FunctionDef:   CheckFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        case AST_VariableDecl:  CheckVariableDecl(ctx, node); break;
        default:
            INVALID_CODE_PATH;
    }
}

b32 Check(Sem_Check_Context *ctx)
{
    Ast_Node *root = ctx->ast->root;
    ASSERT(root);
    for (s64 index = 0;
        index < root->node_list.count && ContinueChecking(ctx);
        index++)
    {
        Ast_Node *node = root->node_list.nodes[index];
        CheckTopLevelStmt(ctx, node);
    }
    return HasError(ctx->comp_ctx);
}

} // hplang
