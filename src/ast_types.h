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

    AST_Parameter,

    AST_Type_Plain,
    AST_Type_Pointer,
    AST_Type_Array,

    AST_StructBody,
    // struct field and stuff

    AST_BlockStmt,
    AST_IfStmt,
    AST_ForStmt,
    AST_WhileStmt,
    AST_ReturnStmt,
    AST_AssignStmt,     // <lvalue-expr> = <rvalue-expr>
    AST_ExpressionStmt,
    AST_Expression,

    AST_IntLiteral,
    AST_FloatLiteral,
    AST_CharLiterla,
    AST_StringLiteral,
    AST_VariableRef,
    AST_FunctionCall,
    AST_FunctionCallArgs,

    AST_BinaryExpr,
    AST_UnaryExpr,
    AST_TernaryExpr,      // <expr> ? <exptr> : <expr>
};

enum Ast_Binary_Op
{
    AST_OP_Add,
    AST_OP_Subtract,
    AST_OP_Multiply,
    AST_OP_Divide,
    // TODO(henrik): Add modulo op (should it work for floats too?)
    //AST_OP_Modulo,

    AST_OP_And,
    AST_OP_Or,

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
    AST_OP_Complement,

    AST_OP_Not,

    AST_OP_Address,      // &: Take address of a variable
    AST_OP_Deref,        // @: Dereference pointer
};

enum Ast_Assignment_Op
{
    AST_OP_Assign,

    // Shorthand assigment operators
    AST_OP_AddAssign,
    AST_OP_SubtractAssign,
    AST_OP_MultiplyAssign,
    AST_OP_DivideAssign,
    // TODO(henrik): Add module assignment op
    //AST_OP_ModuloAssign,
};


struct Token;
struct Ast_Node;

struct Ast_Node_List
{
    s64 capacity;
    s64 node_count;
    Ast_Node **nodes;
};

struct Ast_Int_Literal
{
    s64 value;
};

struct Ast_Float_Literal
{
    f64 value;
};

struct Ast_Char_Literal
{
    char value;
};

struct Ast_String_Literal
{
    String value;
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

struct Ast_Variable_Ref
{
    // The name can be looked up from the Ast_Node::token.
    // A general Name type would be good (containing pre computed hash of the
    // name for faster compares or hash map operations).
};

struct Ast_Function_Call
{
    // The name can be looked up from the Ast_Node::token.
    Ast_Node *args;
};

struct Ast_Expression
{
    union {
        Ast_Int_Literal     int_literal;
        Ast_Float_Literal   float_literal;
        Ast_Char_Literal    char_literal;
        Ast_String_Literal  string_literal;
        Ast_Binary_Expr     binary_expr;
        Ast_Unary_Expr      unary_expr;
        Ast_Function_Call   function_call;
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
    const Token *name;  // NOTE(henrik): This is optional, so may be null.
    const Token *module_name;
};

struct Ast_Function
{
    const Token *name;
    Ast_Node_List parameters;
    Ast_Node *return_type;
    Ast_Node *body;
};

struct Ast_Type_Node
{
    /*enum Kind
    {
        Kind_Pointer,
        Kind_Array,
        Kind_Plain
    };
    Kind kind;*/
    struct Pointer {
        s64 indirection;
        Ast_Node *base_type;
    };
    struct Array {
        s64 array;
        Ast_Node *base_type;
    };
    struct Plain {
        // uses Ast_Node::token
    };
    union {
        Pointer pointer;
        Array array;
        Plain plain;
    };
};

struct Ast_Parameter
{
    const Token *name;
    Ast_Node *type;
};

struct Ast_If_Stmt
{
    Ast_Node *condition_expr;
    Ast_Node *true_stmt;
    Ast_Node *false_stmt;
};

struct Ast_Return_Stmt
{
    Ast_Node *expression;
};

struct Ast_Node
{
    Ast_Type type;
    union {
        Ast_Node_List   node_list;
        Ast_Import      import;
        Ast_Function    function;
        Ast_Parameter   parameter;
        Ast_If_Stmt     if_stmt;
        Ast_Return_Stmt return_stmt;
        Ast_Assignment  assignment;
        Ast_Expression  expression;
        Ast_Type_Node   type_node;
    };
    const Token *token;
};

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node);

} // hplang

#define H_HPLANG_AST_TYPES_H
#endif

