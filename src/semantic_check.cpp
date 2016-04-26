
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
    ctx.env = &comp_ctx->env;
    ctx.open_file = open_file;
    ctx.comp_ctx = comp_ctx;
    return ctx;
}

void FreeSemanticCheckContext(Sem_Check_Context *ctx)
{
    FreeMemoryArena(&ctx->temp_arena);
    ctx->ast = nullptr;
}

// Semantic check
// --------------

static b32 ContinueChecking(Sem_Check_Context *ctx)
{
    return ContinueCompiling(ctx->comp_ctx);
}

static void PrintString(FILE *file, String str)
{
    fwrite(str.data, 1, str.size, file);
}

static void Error(Sem_Check_Context *ctx, Ast_Node *node, const char *message)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "%s\n", message);
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void ErrorSymbolNotTypename(Sem_Check_Context *ctx, Ast_Node *node, Name name)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Symbol '");
    PrintString(err_ctx->file, name.str);
    fprintf(err_ctx->file, "' is not a typename\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void ErrorImport(Sem_Check_Context *ctx, Ast_Node *node, String filename)
{
    Error_Context *err_ctx = &ctx->comp_ctx->error_ctx;
    AddError(err_ctx, node->file_loc);
    PrintFileLocation(err_ctx->file, node->file_loc);
    fprintf(err_ctx->file, "Could not open file '");
    PrintString(err_ctx->file, filename);
    fprintf(err_ctx->file, "'\n");
    PrintSourceLineAndArrow(ctx->comp_ctx, ctx->open_file, node->file_loc);
}

static void CheckImport(Sem_Check_Context *ctx, Ast_Node *node)
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

static Type* CheckType(Sem_Check_Context *ctx, Ast_Node *node)
{
    if (!node) return nullptr;
    switch (node->type)
    {
    case AST_Type_Plain:
        {
            Symbol *symbol = LookupSymbol(ctx->env, node->type_node.plain.name);
            if (symbol)
            {
                switch (symbol->sym_type)
                {
                    case SYM_Module:
                    case SYM_Function:
                    case SYM_ForeignFunction:
                    case SYM_Constant:
                    case SYM_Variable:
                    case SYM_Parameter:
                    case SYM_Member:
                        ErrorSymbolNotTypename(ctx, node, node->type_node.plain.name);
                        break;

                    case SYM_PrimitiveType:
                        return symbol->type;
                    case SYM_Struct:
                    //case SYM_Enum:
                    //case SYM_Typealias:
                        NOT_IMPLEMENTED("struct, enum and typealias in CheckType");
                        break;
                }
            }
        } break;
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

static Type* CheckFunctionCall(Sem_Check_Context *ctx, Ast_Node *node)
{
    NOT_IMPLEMENTED("function call expression checking");
    return nullptr;
}

static Type* CheckExpression(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_Null:
            return GetBuiltinType(TYP_null);
        case AST_BoolLiteral:
            return GetBuiltinType(TYP_bool);
        case AST_CharLiteral:
            return GetBuiltinType(TYP_char);
        case AST_IntLiteral:
            return GetBuiltinType(TYP_int_lit);
        case AST_Float32Literal:
            return GetBuiltinType(TYP_f32);
        case AST_Float64Literal:
            return GetBuiltinType(TYP_f64);
        case AST_StringLiteral:
            NOT_IMPLEMENTED("string literal expression checking");
            return nullptr;

        case AST_VariableRef:
            NOT_IMPLEMENTED("variable reference expression checking");
            return nullptr;
        case AST_FunctionCall:
            return CheckFunctionCall(ctx, node);

        case AST_AssignmentExpr:
        case AST_BinaryExpr:
        case AST_UnaryExpr:
        case AST_TernaryExpr:
            break;
        default:
            INVALID_CODE_PATH;
    }
    NOT_IMPLEMENTED("expression checking");
    return nullptr;
}

static void CheckVariableDecl(Sem_Check_Context *ctx, Ast_Node *node)
{
    Type *type = CheckType(ctx, node->variable_decl.type);
    Type *init_type = CheckExpression(ctx, node->variable_decl.init);
    if (!type && !init_type)
    {
        Error(ctx, node, "Variable type cannot be inferred");
    }
    if (!type) type = init_type;
    if (type != init_type)
    {
        Error(ctx, node, "Variable init type is not same");
    }
}

static void CheckBlockStatement(Sem_Check_Context *ctx, Ast_Node *node);

static void CheckStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_BlockStmt:
            CheckBlockStatement(ctx, node);
            break;
        case AST_IfStmt:
            //CheckIfStatement(ctx, node);
            break;
        case AST_ForStmt:
            //CheckForStatement(ctx, node);
            break;
        case AST_WhileStmt:
            //CheckWhileStatement(ctx, node);
            break;
        case AST_ReturnStmt:
            //CheckReturnStatement(ctx, node);
            break;
        case AST_VariableDecl:
            CheckVariableDecl(ctx, node);
            break;
        default:
            CheckExpression(ctx, node);
            break;

        case AST_TopLevel:
        case AST_Import:
        case AST_ForeignBlock:
        case AST_FunctionDef:
        case AST_StructDef:
        case AST_Parameter:
            INVALID_CODE_PATH;
    }
}

static void CheckBlockStatement(Sem_Check_Context *ctx, Ast_Node *node)
{
    s64 count = node->block.statements.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *stmt = node->block.statements.nodes[i];
        CheckStatement(ctx, stmt);
    }
}

static void CheckParameters(Sem_Check_Context *ctx, Ast_Node *node, Type *ftype)
{
    s64 count = node->function.parameters.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *param = node->function.parameters.nodes[i];
        Symbol *old_sym = LookupSymbolInCurrentScope(
                            ctx->env,
                            param->parameter.name);
        if (old_sym)
        {
            ASSERT(old_sym->sym_type == SYM_Parameter);
            Error(ctx, param, "Parameter already declared");
        }

        Type *param_type = CheckType(ctx, param->parameter.type);
        AddSymbol(ctx->env, SYM_Parameter, param->parameter.name, param_type);

        ftype->function_type.parameter_types[i] = param_type;
    }
}

static void CheckFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function.name.str.data);

    Type *ftype = PushStruct<Type>(&ctx->env->arena);
    s64 param_count = node->function.parameters.count;
    ftype->function_type.parameter_count = param_count;
    ftype->function_type.parameter_types = PushArray<Type*>(&ctx->env->arena, param_count);

    ftype->function_type.return_type = CheckType(ctx, node->function.return_type);

    AddSymbol(ctx->env, SYM_Function, node->function.name, ftype);

    OpenScope(ctx->env);

    CheckParameters(ctx, node, ftype);
    CheckBlockStatement(ctx, node->function.body);

    CloseScope(ctx->env);
}

static void CheckStructMember(Sem_Check_Context *ctx, Ast_Node *node, StructType *s)
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

static void CheckStruct(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->struct_def.name.str.data);
    s64 count = node->struct_def.members.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        CheckStructMember(ctx, node->struct_def.members.nodes[i], nullptr);
    }
}

static void CheckForeignFunction(Sem_Check_Context *ctx, Ast_Node *node)
{
    ASSERT(node->function.name.str.data);

    Type *ftype = PushStruct<Type>(&ctx->env->arena);
    s64 param_count = node->function.parameters.count;
    ftype->function_type.parameter_count = param_count;
    ftype->function_type.parameter_types = PushArray<Type*>(&ctx->env->arena, param_count);

    ftype->function_type.return_type = CheckType(ctx, node->function.return_type);

    AddSymbol(ctx->env, SYM_ForeignFunction, node->function.name, ftype);

    CheckParameters(ctx, node, ftype);
}

static void CheckForeignBlockStmt(Sem_Check_Context *ctx, Ast_Node *node)
{
    switch (node->type)
    {
        case AST_FunctionDef:   CheckForeignFunction(ctx, node); break;
        case AST_StructDef:     CheckStruct(ctx, node); break;
        default:
            INVALID_CODE_PATH;
    }
}

static void CheckForeignBlock(Sem_Check_Context *ctx, Ast_Node *node)
{
    s64 count = node->foreign.statements.count;
    for (s64 i = 0; i < count && ContinueChecking(ctx); i++)
    {
        Ast_Node *stmt = node->foreign.statements.nodes[i];
        CheckForeignBlockStmt(ctx, stmt);
    }
}

static void CheckTopLevelStmt(Sem_Check_Context *ctx, Ast_Node *node)
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

    Ast_Node_List *statements = &root->top_level.statements;
    for (s64 index = 0;
        index < statements->count && ContinueChecking(ctx);
        index++)
    {
        Ast_Node *node = statements->nodes[index];
        CheckTopLevelStmt(ctx, node);
    }
    return HasError(ctx->comp_ctx);
}

} // hplang
