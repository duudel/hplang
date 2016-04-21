
#include "ast_types.h"
#include "token.h"
#include "memory.h"
#include "assert.h"

namespace hplang
{

static b32 GrowNodeList(Ast_Node_List *nodes)
{
    ASSERT(nodes->capacity >= 0);
    s64 old_capacity = nodes->capacity;
    s64 new_capacity = old_capacity + old_capacity / 2;
    if (new_capacity < 8) new_capacity = 8;

    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node*);
    nodes_p = Realloc(nodes_p, new_capacity * sizeof(Ast_Node*));
    if (nodes_p.ptr)
    {
        nodes->capacity = new_capacity;
        nodes->nodes = (Ast_Node**)nodes_p.ptr;
        return true;
    }
    ASSERT(0 && "SHOULD NOT HAPPEN IN NORMAL USE");
    return false;
}

void FreeNodeList(Ast_Node_List *nodes)
{
    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node*);
    Free(nodes_p);
}

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node)
{
    if (nodes->count >= nodes->capacity)
    {
        if (!GrowNodeList(nodes))
            return false;
    }
    nodes->nodes[nodes->count++] = node;
    return true;
}

// AST
// ---

Ast_Node* PushAstNode(Ast *ast, Ast_Node_Type node_type, const Token *token)
{
    Ast_Node *node = PushStruct<Ast_Node>(&ast->arena);
    *node = { };
    node->type = node_type;
    node->token = token;
    return node;
}

static void FreeAstNode(Ast_Node *node);

static void FreeAstNodeList(Ast_Node_List *node_list)
{
    for (s64 i = 0; i < node_list->count; i++)
    {
        Ast_Node *node = node_list->nodes[i];
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
            FreeAstNodeList(&node->node_list);
            break;

        case AST_Import:
            break;

        case AST_ForeignBlock:
            FreeAstNodeList(&node->node_list);
            break;

        case AST_VariableDecl:
            FreeAstNode(node->variable_decl.type);
            FreeAstNode(node->variable_decl.init);
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

        case AST_StructMember:
            FreeAstNode(node->struct_member.type);
            break;

        case AST_BlockStmt:
            FreeAstNodeList(&node->node_list);
            break;

        case AST_IfStmt:
            FreeAstNode(node->if_stmt.condition_expr);
            FreeAstNode(node->if_stmt.true_stmt);
            FreeAstNode(node->if_stmt.false_stmt);
            break;

        case AST_ForStmt:
            FreeAstNode(node->for_stmt.range_expr);
            FreeAstNode(node->for_stmt.init_expr);
            FreeAstNode(node->for_stmt.condition_expr);
            FreeAstNode(node->for_stmt.increment_expr);
            FreeAstNode(node->for_stmt.loop_stmt);
            break;

        case AST_WhileStmt:
            FreeAstNode(node->while_stmt.condition_expr);
            FreeAstNode(node->while_stmt.loop_stmt);
            break;

        case AST_ReturnStmt:
            FreeAstNode(node->return_stmt.expression);
            break;

        case AST_Null:
        case AST_BoolLiteral:
        case AST_IntLiteral:
        case AST_Float32Literal:
        case AST_Float64Literal:
        case AST_CharLiterla:
        case AST_StringLiteral:
        case AST_VariableRef:
            break;

        case AST_FunctionCall:
            FreeAstNode(node->expression.function_call.args);
            break;
        case AST_FunctionCallArgs:
            FreeAstNodeList(&node->node_list);
            break;

        case AST_AssignmentExpr:
            FreeAstNode(node->expression.assignment.left);
            FreeAstNode(node->expression.assignment.right);
            break;
        case AST_BinaryExpr:
            FreeAstNode(node->expression.binary_expr.left);
            FreeAstNode(node->expression.binary_expr.right);
            break;
        case AST_UnaryExpr:
            FreeAstNode(node->expression.unary_expr.expr);
            break;
        case AST_TernaryExpr:
            FreeAstNode(node->expression.ternary_expr.condition_expr);
            FreeAstNode(node->expression.ternary_expr.expr_a);
            FreeAstNode(node->expression.ternary_expr.expr_b);
            break;
    }
}

void FreeAst(Ast *ast)
{
    FreeAstNode(ast->root);
    FreeMemoryArena(&ast->arena);
    ast->root = nullptr;
    if (ast->tokens)
    {
        FreeTokenList(ast->tokens);
        ast->tokens = nullptr;
    }
}

} // hplang
