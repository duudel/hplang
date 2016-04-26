#ifndef H_HPLANG_SYMBOLS_H

#include "types.h"
#include "array.h"

namespace hplang
{

enum Type_Tag
{
    //TYP_pointer,
    TYP_null,
    TYP_int_lit,

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

    TYP_Function,
    TYP_Struct,
    //TYP_Enum,
};

struct Type;

struct StructMember
{
    Name name;
    Type *type;
};

//typedef Array<StructMember> Member_List;

struct StructType
{
    Name name;
    s64 count;
    StructMember **members;
    //Member_List members;
};

//typedef Array<Type*> Type_List;

struct FunctionType
{
    Type *return_type;
    s64 parameter_count;
    Type **parameter_types;
    //Type_List parameter_types;
};

struct Type
{
    Type_Tag tag;
    s32 size;
    s32 alignment;
    s32 pointer;
    union {
        FunctionType    function_type;
        StructType      struct_type;
    };
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

    SYM_PrimitiveType,
    SYM_Struct,
    //SYM_Enum,
    //SYM_Typealias
};

struct Scope;

struct Symbol
{
    Symbol_Type sym_type;
    Name name;
    Type *type;
    Scope *scope;
};

struct Scope
{
    static const s64 INITIAL_SYM_TABLE_SIZE = 1021; // Prime number

    s64 table_size;
    Array<Symbol*> table;

    Scope *parent;
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

Symbol* AddSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type);
Symbol* LookupSymbol(Environment *env, Name name);
Symbol* LookupOverloadedSymbol(Environment *env, Name name, Type *type);
Symbol* LookupSymbolInCurrentScope(Environment *env, Name name);

} // hplang

#define H_HPLANG_SYMBOLS_H
#endif
