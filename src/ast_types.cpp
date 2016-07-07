
#include "ast_types.h"
#include "token.h"
#include "memory.h"
#include "assert.h"

#include <cstdio>   // TODO(henrik): Add "io.impl.h" that includes cstdio. It
                    // could also define something like
                    //   FILE* ToFile(IoFile *f) { return (FILE*)f; }
#include <cinttypes> // For PRId64

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
            FreeAstNodeList(&node->function_def.parameters);
            FreeAstNode(node->function_def.body);
            break;

        case AST_FunctionDecl:
            FreeAstNodeList(&node->function_decl.parameters);
            break;

        case AST_StructDef:
            FreeAstNodeList(&node->struct_def.members);
            break;

        case AST_Typealias:
            FreeAstNode(node->typealias.type);
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

        case AST_BreakStmt:
        case AST_ContinueStmt:
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
            FreeAstExpr(expr->access_expr.base);
            FreeAstExpr(expr->access_expr.member);
            break;
        case AST_SubscriptExpr:
            FreeAstExpr(expr->subscript_expr.base);
            FreeAstExpr(expr->subscript_expr.index);
            break;
        case AST_TypecastExpr:
            FreeAstExpr(expr->typecast_expr.expr);
            FreeAstNode(expr->typecast_expr.type);
            break;
        case AST_AlignOf:
            FreeAstNode(expr->alignof_expr.type);
            break;
        case AST_SizeOf:
            FreeAstNode(expr->sizeof_expr.type);
            break;
    }
}

void FreeAst(Ast *ast)
{
    FreeAstNode(ast->root);
    FreeMemoryArena(&ast->arena);
    ast->root = nullptr;
}


// Ast printing

static void PrintRootPadding(IoFile *file, s64 level, s64 parent_level, s64 second_parent)
{
    for (s64 i = 0; i < level; i++)
    {
        if (i > parent_level)
        { fprintf((FILE*)file, "--"); }
        else if (i == parent_level)
        { fprintf((FILE*)file, "+-"); }
        else if (i == second_parent)
        { fprintf((FILE*)file, "| "); }
        else
        { fprintf((FILE*)file, "  "); }
    }
}

static void PrintPadding(IoFile *file, s64 level, s64 parent_level, s64 second_parent)
{
    for (s64 i = 0; i < level; i++)
    {
        if (i == parent_level || i == second_parent)
        { fprintf((FILE*)file, "| "); }
        else
        { fprintf((FILE*)file, "  "); }
    }
}

static void PrintOp(IoFile *file, Assignment_Op op)
{
    FILE *f = (FILE*)file;
    switch (op)
    {
        case AS_OP_Assign:              fprintf(f, "="); break;
        case AS_OP_AddAssign:           fprintf(f, "+="); break;
        case AS_OP_SubtractAssign:      fprintf(f, "-="); break;
        case AS_OP_MultiplyAssign:      fprintf(f, "*="); break;
        case AS_OP_DivideAssign:        fprintf(f, "/="); break;
        case AS_OP_ModuloAssign:        fprintf(f, "%%="); break;
        case AS_OP_LeftShiftAssign:     fprintf(f, "<<="); break;
        case AS_OP_RightShiftAssign:    fprintf(f, ">>="); break;
        case AS_OP_BitAndAssign:        fprintf(f, "&="); break;
        case AS_OP_BitOrAssign:         fprintf(f, "|="); break;
        case AS_OP_BitXorAssign:        fprintf(f, "^="); break;
    }
}

static void PrintOp(IoFile *file, Binary_Op op)
{
    FILE *f = (FILE*)file;
    switch (op)
    {
        case BIN_OP_Add:        fprintf(f, "+"); break;
        case BIN_OP_Subtract:   fprintf(f, "-"); break;
        case BIN_OP_Multiply:   fprintf(f, "*"); break;
        case BIN_OP_Divide:     fprintf(f, "/"); break;
        case BIN_OP_Modulo:     fprintf(f, "%%"); break;
        case BIN_OP_LeftShift:  fprintf(f, "<<"); break;
        case BIN_OP_RightShift: fprintf(f, ">>"); break;
        case BIN_OP_BitAnd:     fprintf(f, "&"); break;
        case BIN_OP_BitOr:      fprintf(f, "|"); break;
        case BIN_OP_BitXor:     fprintf(f, "^"); break;
        case BIN_OP_And:        fprintf(f, "&&"); break;
        case BIN_OP_Or:         fprintf(f, "||"); break;
        case BIN_OP_Equal:      fprintf(f, "=="); break;
        case BIN_OP_NotEqual:   fprintf(f, "!="); break;
        case BIN_OP_Less:       fprintf(f, "<"); break;
        case BIN_OP_LessEq:     fprintf(f, "<="); break;
        case BIN_OP_Greater:    fprintf(f, ">"); break;
        case BIN_OP_GreaterEq:  fprintf(f, ">="); break;
        case BIN_OP_Range:      fprintf(f, ".."); break;
    }
}

static void PrintOp(IoFile *file, Unary_Op op)
{
    FILE *f = (FILE*)file;
    switch (op)
    {
        case UN_OP_Positive:    fprintf(f, "+"); break;
        case UN_OP_Negative:    fprintf(f, "-"); break;
        case UN_OP_Complement:  fprintf(f, "~"); break;
        case UN_OP_Not:         fprintf(f, "!"); break;
        case UN_OP_Address:     fprintf(f, "&"); break;
        case UN_OP_Deref:       fprintf(f, "@"); break;
    }
}

static void PrintNode(IoFile *file, Ast_Node *node, s64 level, s64 lev_diff, s64 prev_parent);

static void PrintExpr(IoFile *file, Ast_Expr *expr, s64 level, s64 lev_diff, s64 prev_parent)
{
    FILE *f = (FILE*)file;
    s64 parent_level = level;
    level += lev_diff;
    PrintRootPadding(file, level, parent_level, prev_parent);
    switch (expr->type)
    {
        case AST_Null:
            fprintf(f, "<null>\n");
            break;
        case AST_BoolLiteral:
            fprintf(f, "<%s>\n", (expr->bool_literal.value) ? "true" : "false");
            break;
        case AST_CharLiteral:
            fprintf(f, "'%c'\n", expr->char_literal.value);
            break;
        case AST_IntLiteral:
            fprintf(f, "%" PRIu64 "\n", expr->int_literal.value);
            break;
        case AST_UIntLiteral:
            fprintf(f, "%" PRIu64 "\n", expr->uint_literal.value);
            break;
        case AST_Float32Literal:
            fprintf(f, "%f\n", expr->float32_literal.value);
            break;
        case AST_Float64Literal:
            fprintf(f, "%f\n", expr->float64_literal.value);
            break;
        case AST_StringLiteral:
            PrintString(file, expr->string_literal.value);
            fprintf(f, "\n");
            break;
        case AST_VariableRef:
            fprintf(f, "<var_ref> ");
            PrintName(file, expr->variable_ref.name);
            fprintf(f, "\n");
            break;
        case AST_FunctionCall:
            fprintf(f, "<func_call>");
            fprintf(f, "\n");
            PrintExpr(file, expr->function_call.fexpr, level, 1, parent_level);
            fprintf(f, "\n");
            break;

        case AST_AssignmentExpr:
            fprintf(f, "<assignment> ");
            PrintOp(file, expr->assignment.op);
            fprintf(f, "\n");
            PrintExpr(file, expr->assignment.left, level, 1, parent_level);
            PrintExpr(file, expr->assignment.right, level, 1, parent_level);
            break;
        case AST_BinaryExpr:
            fprintf(f, "<binary_expr> ");
            PrintOp(file, expr->binary_expr.op);
            fprintf(f, "\n");
            PrintExpr(file, expr->binary_expr.left, level, 1, parent_level);
            PrintExpr(file, expr->binary_expr.right, level, 1, parent_level);
            break;
        case AST_UnaryExpr:
            fprintf(f, "<unary_expr> ");
            PrintOp(file, expr->unary_expr.op);
            fprintf(f, "\n");
            PrintExpr(file, expr->unary_expr.expr, level, 1, parent_level);
            break;
        case AST_TernaryExpr:
            fprintf(f, "<ternary_expr>");
            fprintf(f, "\n");
            PrintExpr(file, expr->ternary_expr.cond_expr, level, 1, parent_level);
            PrintExpr(file, expr->ternary_expr.true_expr, level, 1, parent_level);
            PrintExpr(file, expr->ternary_expr.false_expr, level, 1, parent_level);
            break;
        case AST_AccessExpr:
            fprintf(f, "<access_expr>");
            fprintf(f, "\n");
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "base:\n");
            PrintExpr(file, expr->access_expr.base, level, 2, parent_level);
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "member:\n");
            PrintExpr(file, expr->access_expr.member, level, 2, parent_level);
            break;
        case AST_SubscriptExpr:
            fprintf(f, "<subscript_expr>");
            fprintf(f, "\n");
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "base:\n");
            PrintExpr(file, expr->subscript_expr.base, level, 2, parent_level);
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "index:\n");
            PrintExpr(file, expr->subscript_expr.index, level, 2, parent_level);
            break;
        case AST_TypecastExpr:
            fprintf(f, "<typecast_expr>\n");
            break;
        case AST_AlignOf:
            fprintf(f, "<alignof_expr>\n");
            PrintPadding(file, level + 1, level, parent_level);
            PrintNode(file, expr->alignof_expr.type, level, 1, parent_level);
            break;
        case AST_SizeOf:
            fprintf(f, "<sizeof_expr>\n");
            PrintPadding(file, level + 1, level, parent_level);
            PrintNode(file, expr->sizeof_expr.type, level, 1, parent_level);
            break;
    }
}

static void PrintNode(IoFile *file, Ast_Node *node, s64 level, s64 lev_diff, s64 prev_parent)
{
    FILE *f = (FILE*)file;
    s64 parent_level = level;
    level += lev_diff;
    PrintRootPadding(file, level, parent_level, prev_parent);
    switch (node->type)
    {
        case AST_TopLevel: INVALID_CODE_PATH; break;

        case AST_Import:
            fprintf(f, "<import> ");
            PrintName(file, node->import.name);
            fprintf(f, " :: ");
            PrintString(file, node->import.module_name);
            fprintf(f, "\n");
            break;
        case AST_ForeignBlock:
            fprintf(f, "<foreign>");
            fprintf(f, "\n");
            for (s64 i = 0; i < node->foreign.statements.count; i++)
            {
                Ast_Node *stmt = node->foreign.statements[i];
                PrintNode(file, stmt, level, 1, parent_level);
            }
            fprintf(f, "\n");
            break;
        case AST_VariableDecl:
            {
                fprintf(f, "<var_decl> ");
                PrintName(file, node->variable_decl.names.name);
                Ast_Variable_Decl_Names *names = node->variable_decl.names.next;
                while (names)
                {
                    fprintf(f, ", ");
                    PrintName(file, names->name);
                    names = names->next;
                }
                fprintf(f, "\n");
                if (node->variable_decl.type)
                {
                    PrintPadding(file, level + 1, level, parent_level);
                    fprintf(f, "type:\n");
                    PrintNode(file, node->variable_decl.type, level, 2, parent_level);
                }
                if (node->variable_decl.init_expr)
                {
                    PrintPadding(file, level + 1, level, parent_level);
                    fprintf(f, "init expr:\n");
                    PrintExpr(file, node->variable_decl.init_expr, level, 2, parent_level);
                }
            } break;
        case AST_FunctionDef:
            fprintf(f, "<func_def> ");
            PrintName(file, node->function_def.name);
            fprintf(f, "\n");
            if (node->function_def.return_type)
            {
                PrintPadding(file, level + 1, level, parent_level);
                fprintf(f, "return type:\n");
                PrintNode(file, node->function_decl.return_type, level, 2, parent_level);
            }
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "parameter types:\n");
            for (s64 i = 0; i < node->function_decl.parameters.count; i++)
            {
                Ast_Node *param = node->function_decl.parameters[i];
                PrintNode(file, param, level, 2, prev_parent);
            }
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "body:\n");
            PrintNode(file, node->function_def.body, level, 2, parent_level);
            fprintf(f, "\n");
            break;
        case AST_FunctionDecl:
            fprintf(f, "<func_decl> ");
            PrintName(file, node->function_decl.name);
            fprintf(f, "\n");
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "return type:\n");
            PrintNode(file, node->function_decl.return_type, level, 2, parent_level);
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "parameter types:\n");
            for (s64 i = 0; i < node->function_decl.parameters.count; i++)
            {
                Ast_Node *param = node->function_decl.parameters[i];
                PrintNode(file, param, level, 2, parent_level);
            }
            fprintf(f, "\n");
            break;
        case AST_Parameter:
            fprintf(f, "<parameter> ");
            PrintName(file, node->parameter.name);
            fprintf(f, "\n");
            PrintNode(file, node->parameter.type, level, 1, parent_level);
            break;
        case AST_StructDef:
            fprintf(f, "<struct_def> ");
            PrintName(file, node->struct_def.name);
            fprintf(f, "\n");
            for (s64 i = 0; i < node->struct_def.members.count; i++)
            {
                Ast_Node *member = node->struct_def.members[i];
                PrintNode(file, member, level, 1, parent_level);
            }
            fprintf(f, "\n");
            break;
        case AST_StructMember:
            fprintf(f, "<struct_member> ");
            PrintName(file, node->struct_member.name);
            fprintf(f, "\n");
            PrintNode(file, node->struct_member.type, level, 1, parent_level);
            break;
        case AST_Typealias:
            fprintf(f, "<typealias>");
            PrintName(file, node->typealias.name);
            fprintf(f, "\n");
            PrintNode(file, node->typealias.type, level, 1, parent_level);
            break;

        case AST_Type_Plain:
            fprintf(f, "<type> ");
            PrintName(file, node->type_node.plain.name);
            fprintf(f, "\n");
            break;
        case AST_Type_Pointer:
            fprintf(f, "<ptr_type> *");
            fprintf(f, "%" PRId64 "\n", node->type_node.pointer.indirection);
            PrintNode(file, node->type_node.pointer.base_type, level, 1, parent_level);
            break;
        case AST_Type_Array:
            fprintf(f, "<arr_type> []");
            fprintf(f, "%" PRId64 "\n", node->type_node.array.array);
            PrintNode(file, node->type_node.array.base_type, level, 1, parent_level);
            break;
        case AST_Type_Function:
            {
                fprintf(f, "<func_type>");
                fprintf(f, "\n");
                PrintPadding(file, level + 1, level, parent_level);
                fprintf(f, "return type:\n");
                PrintNode(file, node->type_node.function.return_type, level, 2, parent_level);
                fprintf(f, "\n");
                PrintPadding(file, level + 1, level, parent_level);
                fprintf(f, "parameter types:\n");
                Ast_Param_Type *pt = node->type_node.function.param_types;
                while (pt)
                {
                    PrintNode(file, pt->type, level, 2, parent_level);
                    pt = pt->next;
                }
            } break;

        case AST_BlockStmt:
            fprintf(f, "<block_stmt>");
            fprintf(f, "\n");
            for (s64 i = 0; i < node->block_stmt.statements.count; i++)
            {
                Ast_Node *stmt = node->block_stmt.statements[i];
                PrintNode(file, stmt, level, 1, parent_level);
            }
            fprintf(f, "\n");
            break;
        case AST_IfStmt:
            fprintf(f, "<if_stmt>");
            fprintf(f, "\n");
            PrintExpr(file, node->if_stmt.cond_expr, level, 1, parent_level);
            PrintPadding(file, level + 1, level, parent_level);
            fprintf(f, "then:\n");
            PrintNode(file, node->if_stmt.then_stmt, level, 2, parent_level);
            if (node->if_stmt.else_stmt)
            {
                PrintPadding(file, level + 1, level, parent_level);
                fprintf(f, "else:\n");
                PrintNode(file, node->if_stmt.else_stmt, level, 2, parent_level);
            }
            fprintf(f, "\n");
            break;
        case AST_WhileStmt:
            fprintf(f, "<while_stmt>");
            fprintf(f, "\n");
            PrintExpr(file, node->while_stmt.cond_expr, level, 1, parent_level);
            PrintNode(file, node->while_stmt.loop_stmt, level, 2, parent_level);
            fprintf(f, "\n");
            break;
        case AST_ForStmt:
            fprintf(f, "<for_stmt>");
            fprintf(f, "\n");
            break;
        case AST_RangeForStmt:
            fprintf(f, "<range_for_stmt>");
            fprintf(f, "\n");
            break;
        case AST_ReturnStmt:
            fprintf(f, "<return_stmt>");
            fprintf(f, "\n");
            if (node->return_stmt.expr)
                PrintExpr(file, node->return_stmt.expr, level, 1, parent_level);
            break;
        case AST_BreakStmt:
            fprintf(f, "<break>");
            fprintf(f, "\n");
            break;
        case AST_ContinueStmt:
            fprintf(f, "<continue>");
            fprintf(f, "\n");
            break;
        case AST_ExpressionStmt:
            fprintf(f, "<expr_stmt>");
            fprintf(f, "\n");
            PrintExpr(file, node->expr_stmt.expr, level, 1, parent_level);
            break;
    }
}

void PrintAst(IoFile *file, Ast *ast)
{
    Ast_Node_List *statements = &ast->root->top_level.statements;
    for (s64 index = 0;
        index < statements->count;
        index++)
    {
        Ast_Node *node = array::At(*statements, index);
        PrintNode(file, node, 0, 0, 0);
    }
}

} // hplang
