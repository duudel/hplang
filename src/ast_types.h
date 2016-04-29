#ifndef H_HPLANG_AST_TYPES_H

#include "types.h"
#include "memory.h"

namespace hplang
{

enum Ast_Node_Type
{
    AST_TopLevel,

    AST_Import,
    AST_ForeignBlock,
    AST_VariableDecl,
    AST_FunctionDef,    // <ident> :: (<param_list>) : <type> {<stmt_block>}
    AST_StructDef,      // <ident> :: struct {<struct_body>}

    AST_Parameter,

    AST_Type_Plain,
    AST_Type_Pointer,
    AST_Type_Array,

    AST_StructMember,

    AST_BlockStmt,
    AST_IfStmt,
    AST_ForStmt,
    AST_WhileStmt,
    AST_ReturnStmt,

    AST_Null,
    AST_BoolLiteral,
    AST_CharLiteral,
    AST_IntLiteral,
    AST_Float32Literal,
    AST_Float64Literal,
    AST_StringLiteral,
    AST_VariableRef,
    AST_FunctionCall,

    AST_AssignmentExpr,
    AST_BinaryExpr,
    AST_UnaryExpr,
    AST_TernaryExpr,    // <expr> ? <expr> : <expr>
    AST_AccessExpr,     // <expr>.<variable_ref>
    AST_TypecastExpr,   // <expr> -> <type>
};

// TODO(henrik): Add bit shift operators
// << and >> (shift left and right)
// maybe also
// <<< and >>> (rotate left and right)
enum Binary_Op
{
    BIN_OP_Add,
    BIN_OP_Subtract,
    BIN_OP_Multiply,
    BIN_OP_Divide,
    BIN_OP_Modulo,

    BIN_OP_BitAnd,
    BIN_OP_BitOr,
    BIN_OP_BitXor,

    BIN_OP_And,
    BIN_OP_Or,

    BIN_OP_Equal,
    BIN_OP_NotEqual,
    BIN_OP_Less,
    BIN_OP_LessEq,
    BIN_OP_Greater,
    BIN_OP_GreaterEq,

    BIN_OP_Range,       // a .. b

    BIN_OP_Subscript,   // a[x]
};

enum Unary_Op
{
    UN_OP_Positive,
    UN_OP_Negative,
    UN_OP_Complement,

    UN_OP_Not,

    UN_OP_Address,      // &: Take address of a variable
    UN_OP_Deref,        // @: Dereference pointer
};

enum Assignment_Op
{
    AS_OP_Assign,

    // Shorthand assigment operators
    AS_OP_AddAssign,
    AS_OP_SubtractAssign,
    AS_OP_MultiplyAssign,
    AS_OP_DivideAssign,
    AS_OP_ModuloAssign,

    AS_OP_BitAndAssign,
    AS_OP_BitOrAssign,
    AS_OP_BitXorAssign,
};


struct Token;
struct Ast_Node;

struct Ast_Node_List
{
    s64 capacity;
    s64 count;
    Ast_Node **nodes;
};

struct Ast_Bool_Literal
{ bool value; };

struct Ast_Int_Literal
{ s64 value; };

struct Ast_Float64_Literal
{ f64 value; };

struct Ast_Float32_Literal
{ f32 value; };

struct Ast_Char_Literal
{ char value; };

struct Ast_String_Literal
{ String value; };

struct Ast_Binary_Expr
{
    Binary_Op op;
    Ast_Node *left, *right;
};

struct Ast_Unary_Expr
{
    Unary_Op op;
    Ast_Node *expr;
};

struct Ast_Ternary_Expr
{
    Ast_Node *condition_expr;
    Ast_Node *true_expr;
    Ast_Node *false_expr;
};

struct Ast_Access_Expr
{
    Ast_Node *left, *right;
};

struct Ast_Typecast_Expr
{
    Ast_Node *expr;
    Ast_Node *type;
};

struct Ast_Variable_Ref
{
    Name name;
};

struct Ast_Function_Call
{
    Ast_Node *fexpr;
    Ast_Node_List args;
};

struct Ast_Assignment
{
    Assignment_Op op;
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
        Ast_Ternary_Expr    ternary_expr;
        Ast_Access_Expr     access_expr;
        Ast_Typecast_Expr   typecast_expr;
        Ast_Variable_Ref    variable_ref;
        Ast_Function_Call   function_call;
        Ast_Assignment      assignment;
    };
};

struct Ast_Type_Node
{
    struct Pointer {
        s64 indirection;
        Ast_Node *base_type;
    };
    struct Array {
        s64 array;
        Ast_Node *base_type;
    };
    struct Plain {
        Name name;
    };
    union {
        Pointer pointer;
        Array array;
        Plain plain;
    };
};

struct Ast_Top_Level
{
    Ast_Node_List statements;
};

struct Ast_Foreign_Block
{
    Ast_Node_List statements;
};

struct Ast_Import
{
    // import "module_name";
    // or
    // name :: import "module_name";
    Name name;  // NOTE(henrik): This is optional, so may be empty.
    String module_name;
};

struct Ast_Function_Def
{
    Name name;
    Ast_Node_List parameters;
    Ast_Node *return_type; // NOTE(henrik): This is optional, so may be null
    Ast_Node *body; // TODO(henrik): should use Ast_Node_List here?
};

struct Ast_Parameter
{
    Name name;
    Ast_Node *type;
};

struct Ast_Struct_Def
{
    Name name;
    Ast_Node_List members;
};

struct Ast_Struct_Member
{
    Name name;
    Ast_Node *type; // NOTE(henrik): type can not be null
};

struct Ast_Variable_Decl
{
    Name name;
    Ast_Node *type; // NOTE(henrik): type can be null (type will be inferred)
    Ast_Node *init; // NOTE(henrik): init_expr can be null
};

struct Ast_If_Stmt
{
    Ast_Node *condition_expr;
    Ast_Node *true_stmt;
    Ast_Node *false_stmt;
};

struct Ast_While_Stmt
{
    Ast_Node *condition_expr;
    Ast_Node *loop_stmt;
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

struct Ast_Block_Stmt
{
    Ast_Node_List statements;
};

struct Ast_Node
{
    Ast_Node_Type type;
    union {
        Ast_Top_Level       top_level;
        Ast_Foreign_Block   foreign;
        Ast_Import          import;
        Ast_Function_Def    function;
        Ast_Parameter       parameter;
        Ast_Struct_Def      struct_def;
        Ast_Struct_Member   struct_member;
        Ast_Variable_Decl   variable_decl;
        Ast_If_Stmt         if_stmt;
        Ast_While_Stmt      while_stmt;
        Ast_For_Stmt        for_stmt;
        Ast_Return_Stmt     return_stmt;
        Ast_Block_Stmt      block;
        Ast_Expression      expression;
        Ast_Type_Node       type_node;
    };
    File_Location file_loc;
};

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node);
void FreeNodeList(Ast_Node_List *nodes);

struct Ast
{
    Memory_Arena arena;
    Ast_Node *root;
};

Ast_Node* PushAstNode(Ast *ast, Ast_Node_Type node_type, const Token *token);
void FreeAst(Ast *ast);

} // hplang

#define H_HPLANG_AST_TYPES_H
#endif

