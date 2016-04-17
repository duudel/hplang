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
    AST_ExpressionStmt,
    AST_Expression,

    AST_BoolLiteral,
    AST_IntLiteral,
    AST_Float32Literal,
    AST_Float64Literal,
    AST_CharLiterla,
    AST_StringLiteral,
    AST_VariableRef,
    AST_FunctionCall,
    AST_FunctionCallArgs,

    AST_AssignmentExpr,
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
    // TODO(henrik): Should modulo work for floats too?
    AST_OP_Modulo,

    AST_OP_BitAnd,
    AST_OP_BitOr,
    AST_OP_BitXor,

    AST_OP_And,
    AST_OP_Or,

    AST_OP_Equal,
    AST_OP_NotEqual,
    AST_OP_Less,
    AST_OP_LessEq,
    AST_OP_Greater,
    AST_OP_GreaterEq,

    AST_OP_Range,   // a .. b
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
    AST_OP_ModuloAssign,

    AST_OP_BitAndAssign,
    AST_OP_BitOrAssign,
    AST_OP_BitXorAssign,

    AST_OP_ComplementAssign,
};


struct Token;
struct Ast_Node;

struct Ast_Node_List
{
    s64 capacity;
    s64 node_count;
    Ast_Node **nodes;
};

struct Ast_Bool_Literal
{
    bool value;
};

struct Ast_Int_Literal
{
    s64 value;
};

struct Ast_Float64_Literal
{
    f64 value;
};

struct Ast_Float32_Literal
{
    f32 value;
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
    const Token *name;
    // A general Name type would be good (containing pre computed hash of the
    // name for faster compares or hash map operations).
};

struct Ast_Function_Call
{
    const Token *name;
    Ast_Node *args;
};

struct Ast_Assignment
{
    Ast_Assignment_Op op;
    Ast_Node *left;
    Ast_Node *right;
};

struct Ast_Expression
{
    union {
        Ast_Bool_Literal    bool_literal;
        Ast_Int_Literal     int_literal;
        Ast_Float32_Literal float32_literal;
        Ast_Float64_Literal float64_literal;
        Ast_Char_Literal    char_literal;
        Ast_String_Literal  string_literal;
        Ast_Binary_Expr     binary_expr;
        Ast_Unary_Expr      unary_expr;
        Ast_Variable_Ref    variable_ref;
        Ast_Function_Call   function_call;
        Ast_Assignment      assignment;
    };
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

struct Ast_For_Stmt
{
    Ast_Node *range_expr;

    Ast_Node *init_expr;
    Ast_Node *condition_expr;
    Ast_Node *increment_expr;
    Ast_Node *loop_stmt;
};

struct Ast_Return_Stmt
{
    Ast_Node *expression;
};

struct Ast_Variable_Decl
{
    const Token *name;
    Ast_Node *type; // NOTE(henrik): type can be null (type will be inferred)
    Ast_Node *init; // NOTE(henrik): init_expr can be null
};

struct Ast_Node
{
    Ast_Type type;
    union {
        Ast_Node_List       node_list;
        Ast_Import          import;
        Ast_Function        function;
        Ast_Parameter       parameter;
        Ast_Variable_Decl   variable_decl;
        Ast_If_Stmt         if_stmt;
        Ast_For_Stmt        for_stmt;
        Ast_Return_Stmt     return_stmt;
        Ast_Expression      expression;
        Ast_Type_Node       type_node;
    };
    const Token *token;
};

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node);

} // hplang

#define H_HPLANG_AST_TYPES_H
#endif

