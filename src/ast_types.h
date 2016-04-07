#ifndef H_HPLANG_AST_TYPES_H

#include "types.h"

namespace hplang
{

enum Ast_Type
{
    AST_TopLevel,

    AST_Import,
    AST_VariableDecl,
    AST_FunctionDef,    // <ident> :: (<param_list>) : <type> {<stmt_block>}
    AST_StructDef,      // <ident> :: struct {<struct_body>}

    AST_StructBody,
    AST_StmtBlock,
    AST_AssignStmt,     // <ident> = <expr>
    AST_ExpressionStmt,
    AST_Expression,

    AST_IntLiteral,
    AST_FloatLiteral,
    AST_CharLiterla,
    AST_StringLiteral,
    AST_VariableRef,
    AST_FunctionCall,

    AST_BinaryOp,
    AST_UnaryOp,
    AST_TernaryOp,      // <expr> ? <exptr> : <expr>
};

enum Ast_Binary_Op
{
    AST_OP_Plus,
    AST_OP_Minus,
    AST_OP_Multiply,
    AST_OP_Divide,
    // TODO(henrik): Add modulo op (should it work for floats too?)
    //AST_OP_Modulo,

    AST_OP_Equal,
    AST_OP_NotEqual,
    AST_OP_Less,
    AST_OP_LessEq,
    AST_OP_Greater,
    AST_OP_GreaterEq,
};

enum Ast_Unary_Op
{
    AST_OP_Positive,
    AST_OP_Negative,
    // TODO(henrik): Add bitwise complement op
    //AST_OP_Complement,

    AST_OP_Not,

    AST_OP_Address,      // &: Take address of a variable
    AST_OP_Deref,        // @: Dereference pointer
};

enum Ast_Assignment_Op
{
    AST_OP_Assign,

    // Shorthand assigment operators
    AST_OP_PlusAssign,
    AST_OP_MinusAssign,
    AST_OP_MultiplyAssign,
    AST_OP_DivideAssign,
    // TODO(henrik): Add module assignment op
    //AST_OP_ModuloAssign,
};


struct Token;
struct Ast_Node;

struct Ast_IntLiteral
{
    s64 value;
};

struct Ast_Binary_Expr
{
    Ast_Binary_Op op;
    Ast_Node *left, *right;
};

struct Ast_Unary_Expr
{
    Ast_Unary_Op op;
    Ast_Node *expr;
};

struct Ast_Expression
{
    union {
        Ast_IntLiteral      int_literal;
        Ast_Binary_Expr     binary_expr;
        Ast_Unary_Expr      unary_expr;
    };
};

struct Ast_Assignment
{
    Ast_Assignment_Op op;
    Ast_Node *target;
    Ast_Node *expr;
};

struct Ast_Import
{
    // import "module_name";
    // or
    // name :: import "module_name";
    const Token *name;
    const Token *module_name;
};

struct Ast_Node_List
{
    s64 capacity;
    s64 node_count;
    Ast_Node **nodes;
};

struct Ast_Node
{
    Ast_Type type;
    union {
        Ast_Node_List   node_list;
        Ast_Import      import;
        Ast_Assignment  assignment;
        Ast_Expression  expression;
    };
    const Token *token;
};

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node);

} // hplang

#define H_HPLANG_AST_TYPES_H
#endif

