#ifndef H_HPLANG_SYMBOLS_H

#include "types.h"
#include "array.h"

namespace hplang
{

enum Type_Tag
{
    TYP_null,
    TYP_int_lit,
    TYP_pointer,

    TYP_FIRST_BUILTIN_SYM,

    TYP_void = TYP_FIRST_BUILTIN_SYM,
    TYP_bool,
    TYP_char,
    TYP_u8,
    TYP_s8,
    TYP_u16,
    TYP_s16,
    TYP_u32,
    TYP_s32,
    TYP_u64,
    TYP_s64,
    TYP_f32,
    TYP_f64,
    TYP_string,

    TYP_LAST_BUILTIN = TYP_string,

    TYP_Function,
    TYP_Struct,
    //TYP_Enum,
};

struct Type;

struct Struct_Member
{
    Name name;
    Type *type;
};

struct Struct_Type
{
    Name name;
    s64 member_count;
    Struct_Member *members;
};

struct Function_Type
{
    Type *return_type;
    s64 parameter_count;
    Type **parameter_types;
};

struct Type
{
    Type_Tag tag;
    s32 size;
    s32 alignment;
    s32 pointer;
    union {
        Name            type_name;
        Type            *base_type;
        Function_Type    function_type;
        Struct_Type      struct_type;
    };
};

enum Value_Type
{
    VT_Assignable,
    VT_NonAssignable,
};

Type* GetBuiltinType(Type_Tag tt);

enum Symbol_Type
{
    SYM_Module,
    SYM_Function,
    SYM_ForeignFunction,
    SYM_Constant,
    SYM_Variable,
    SYM_Parameter,
    SYM_Member,

    SYM_Struct,
    //SYM_Enum,
    //SYM_Typealias

    SYM_PrimitiveType,
};

struct Scope;

struct Symbol
{
    Symbol_Type sym_type;
    Name name;
    Type *type;
    Scope *scope;

    Symbol *next_overload;
};

struct Ast_Node;

struct Scope
{
    s64 symbol_count;
    Array<Symbol*> table;

    Scope *parent;

    Type *return_type;      // The return type of the current function scope
    Ast_Node *rt_infer_loc;  // Set if the return type was inferred (location info for errors)
    s64 return_stmt_count;
};

// TODO(henrik): Is there better name for this?
struct Environment
{
    Memory_Arena arena;
    Scope *root; // The global scope of the root module
    Array<Scope*> scopes;

    Scope *current;
};

Environment NewEnvironment();
void FreeEnvironment(Environment *env);

void OpenScope(Environment *env);
void CloseScope(Environment *env);

void OpenFunctionScope(Environment *env, Type *return_type);
// Returns the inferred or declared function type
Type* CloseFunctionScope(Environment *env);

void IncReturnStatements(Environment *env);
Type* GetCurrentReturnType(Environment *env);
Ast_Node* GetCurrentReturnTypeInferLoc(Environment *env);
void InferReturnType(Environment *env, Type *return_type, Ast_Node *location);

Type* PushType(Environment *env, Type_Tag tag);

Symbol* AddSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type);
Symbol* AddFunction(Environment *env, Name name, Type *type);
Symbol* LookupSymbol(Environment *env, Name name);
//Symbol* LookupOverloadedSymbol(Environment *env, Name name, Type *type);
Symbol* LookupSymbolInCurrentScope(Environment *env, Name name);

b32 TypesEqual(Type *a, Type *b);

} // hplang

#define H_HPLANG_SYMBOLS_H
#endif
