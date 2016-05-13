
#include "ast_types.h"
#include "token.h"
#include "memory.h"
#include "assert.h"

namespace hplang
{

void FreeNodeList(Ast_Node_List *nodes)
{
    array::Free(*nodes);
}

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node)
{
    return array::Push(*nodes, node);
}

void FreeExprList(Ast_Expr_List *exprs)
{
    array::Free(*exprs);
}

b32 PushExprList(Ast_Expr_List *exprs, Ast_Expr *expr)
{
    return array::Push(*exprs, expr);
}

// AST
// ---

Ast_Node* PushAstNode(Ast *ast, Ast_Node_Type node_type, File_Location file_loc)
{
    ast->stmt_count++;
    Ast_Node *node = PushStruct<Ast_Node>(&ast->arena);
    *node = { };
    node->type = node_type;
    node->file_loc = file_loc;
    return node;
}

Ast_Node* PushAstNode(Ast *ast, Ast_Node_Type node_type, const Token *token)
{
    return PushAstNode(ast, node_type, token->file_loc);
}

Ast_Expr* PushAstExpr(Ast *ast, Ast_Expr_Type expr_type, File_Location file_loc)
{
    ast->expr_count++;
    Ast_Expr *expr = PushStruct<Ast_Expr>(&ast->arena);
    *expr = { };
    expr->type = expr_type;
    expr->file_loc = file_loc;
    return expr;
}

Ast_Expr* PushAstExpr(Ast *ast, Ast_Expr_Type expr_type, const Token *token)
{
    return PushAstExpr(ast, expr_type, token->file_loc);
}

static void FreeAstExpr(Ast_Expr *expr);

static void FreeAstExprList(Ast_Expr_List *expr_list)
{
    for (s64 i = 0; i < expr_list->count; i++)
    {
        Ast_Expr *expr = array::At(*expr_list, i);
        FreeAstExpr(expr);
    }
    FreeExprList(expr_list);
}

static void FreeAstNode(Ast_Node *node);

static void FreeAstNodeList(Ast_Node_List *node_list)
{
    for (s64 i = 0; i < node_list->count; i++)
    {
        Ast_Node *node = array::At(*node_list, i);
        FreeAstNode(node);
    }
    FreeNodeList(node_list);
}

static void FreeAstNode(Ast_Node *node)
{
    if (!node) return;
    switch (node->type)
    {
        case AST_TopLevel:
            FreeAstNodeList(&node->top_level.statements);
            break;

        case AST_Import:
            break;

        case AST_ForeignBlock:
            FreeAstNodeList(&node->foreign.statements);
            break;

        case AST_VariableDecl:
            FreeAstNode(node->variable_decl.type);
            FreeAstExpr(node->variable_decl.init_expr);
            break;

        case AST_FunctionDef:
            FreeAstNodeList(&node->function.parameters);
            FreeAstNode(node->function.body);
            break;

        case AST_StructDef:
            FreeAstNodeList(&node->struct_def.members);
            break;

        case AST_Parameter:
            FreeAstNode(node->parameter.type);
            break;

        case AST_Type_Plain:
            break;
        case AST_Type_Pointer:
            FreeAstNode(node->type_node.pointer.base_type);
            break;
        case AST_Type_Array:
            FreeAstNode(node->type_node.array.base_type);
            break;
        case AST_Type_Function:
            {
                FreeAstNode(node->type_node.function.return_type);
                Ast_Param_Type *param_type = node->type_node.function.param_types;
                while (param_type)
                {
                    FreeAstNode(param_type->type);
                    param_type = param_type->next;
                }
            } break;

        case AST_StructMember:
            FreeAstNode(node->struct_member.type);
            break;

        case AST_BlockStmt:
            FreeAstNodeList(&node->block_stmt.statements);
            break;

        case AST_IfStmt:
            FreeAstExpr(node->if_stmt.cond_expr);
            FreeAstNode(node->if_stmt.then_stmt);
            FreeAstNode(node->if_stmt.else_stmt);
            break;

        case AST_WhileStmt:
            FreeAstExpr(node->while_stmt.cond_expr);
            FreeAstNode(node->while_stmt.loop_stmt);
            break;

        case AST_ForStmt:
            FreeAstNode(node->for_stmt.init_stmt);
            FreeAstExpr(node->for_stmt.init_expr);
            FreeAstExpr(node->for_stmt.cond_expr);
            FreeAstExpr(node->for_stmt.incr_expr);
            FreeAstNode(node->for_stmt.loop_stmt);
            break;

        case AST_RangeForStmt:
            FreeAstNode(node->range_for_stmt.init_stmt);
            FreeAstExpr(node->range_for_stmt.init_expr);
            FreeAstNode(node->range_for_stmt.loop_stmt);
            break;

        case AST_ReturnStmt:
            FreeAstExpr(node->return_stmt.expr);
            break;

        case AST_ExpressionStmt:
            FreeAstExpr(node->expr_stmt.expr);
            break;
    }
}

void FreeAstExpr(Ast_Expr *expr)
{
    if (!expr) return;
    switch (expr->type)
    {
        case AST_Null:
        case AST_BoolLiteral:
        case AST_CharLiteral:
        case AST_IntLiteral:
        case AST_UIntLiteral:
        case AST_Float32Literal:
        case AST_Float64Literal:
        case AST_StringLiteral:
        case AST_VariableRef:
            break;

        case AST_FunctionCall:
            FreeAstExpr(expr->function_call.fexpr);
            FreeAstExprList(&expr->function_call.args);
            break;

        case AST_AssignmentExpr:
            FreeAstExpr(expr->assignment.left);
            FreeAstExpr(expr->assignment.right);
            break;
        case AST_BinaryExpr:
            FreeAstExpr(expr->binary_expr.left);
            FreeAstExpr(expr->binary_expr.right);
            break;
        case AST_UnaryExpr:
            FreeAstExpr(expr->unary_expr.expr);
            break;
        case AST_TernaryExpr:
            FreeAstExpr(expr->ternary_expr.cond_expr);
            FreeAstExpr(expr->ternary_expr.true_expr);
            FreeAstExpr(expr->ternary_expr.false_expr);
            break;
        case AST_AccessExpr:
            FreeAstExpr(expr->access_expr.left);
            FreeAstExpr(expr->access_expr.right);
            break;
        case AST_TypecastExpr:
            FreeAstExpr(expr->typecast_expr.expr);
            FreeAstNode(expr->typecast_expr.type);
            break;
    }
}

void FreeAst(Ast *ast)
{
    FreeAstNode(ast->root);
    FreeMemoryArena(&ast->arena);
    ast->root = nullptr;
}

} // hplang
