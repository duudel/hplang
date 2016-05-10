#ifndef H_HPLANG_SYMBOLS_H

#include "types.h"
#include "array.h"
#include "io.h"

namespace hplang
{

enum Type_Tag
{
    // A pseudo type pending final inferred type, which will be set as
    // Type::base_type.
    TYP_pending,

    TYP_null,
    TYP_pointer,

    TYP_void,
    TYP_FIRST_BUILTIN_SYM = TYP_void,

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
    s32 size;       // this need not be here
    s32 alignment;  // this need not be here
    union {
        Name            type_name;
        Type            *base_type;
        Function_Type    function_type;
        Struct_Type      struct_type;
    };

    // Cached pointer type; returned by GetPointerType(env, base_type)
    Type *pointer_type;
};

enum Value_Type
{
    VT_Assignable,
    VT_NonAssignable,
};

b32 TypeIsPending(Type *t);
b32 TypeIsNull(Type *t);
b32 TypeIsPointer(Type *t);
b32 TypeIsVoid(Type *t);
b32 TypeIsBoolean(Type *t);
b32 TypeIsChar(Type *t);
b32 TypeIsIntegral(Type *t);
b32 TypeIsFloat(Type *t);
b32 TypeIsNumeric(Type *t);
b32 TypeIsString(Type *t);
b32 TypeIsStruct(Type *t);

b32 TypeIsSigned(Type *t);
b32 TypeIsUnsigned(Type *t);

Type* GetBuiltinType(Type_Tag tt);

void PrintType(IoFile *file, Type *type);
void PrintFunctionType(IoFile *file, Type *return_type, s64 param_count, Type **param_types);


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
    Name unique_name;
    Type *type;

    Symbol *next_overload;
};

struct Ast_Node;

struct Scope
{
    s64 symbol_count;
    Array<Symbol*> table;

    Scope *parent;

    Type *return_type;      // The return type of the current function scope
    Ast_Node *rt_infer_loc; // Set, if the return type was inferred (location info for errors)
    s64 return_stmt_count;
};

// TODO(henrik): Is there better name for this?
struct Environment
{
    Memory_Arena arena;
    Scope *root; // The global scope of the root module
    Array<Scope*> scopes;

    Scope *current;

    s64 unique_id;
};

Environment NewEnvironment();
void FreeEnvironment(Environment *env);

Scope* CurrentScope(Environment *env);
void SetCurrentScope(Environment *env, Scope *scope);

void OpenScope(Environment *env);
void CloseScope(Environment *env);

void OpenFunctionScope(Environment *env, Type *return_type);
// Returns the inferred or declared function return type
Type* CloseFunctionScope(Environment *env);

void IncReturnStatements(Environment *env);
s64 GetReturnStatements(Environment *env);
Type* GetCurrentReturnType(Environment *env);
Ast_Node* GetCurrentReturnTypeInferLoc(Environment *env);
void InferReturnType(Environment *env, Type *return_type, Ast_Node *location);

Type* PushType(Environment *env, Type_Tag tag);
Type* PushPendingType(Environment *env);
Type* GetPointerType(Environment *env, Type *base_type);

Symbol* AddSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type);
Symbol* AddFunction(Environment *env, Name name, Type *type);
Symbol* LookupSymbol(Environment *env, Name name);
Symbol* LookupSymbolInCurrentScope(Environment *env, Name name);

b32 TypesEqual(Type *a, Type *b);

} // hplang

#define H_HPLANG_SYMBOLS_H
#endif
