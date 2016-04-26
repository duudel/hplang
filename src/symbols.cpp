
#include "common.h"
#include "symbols.h"
#include "assert.h"

namespace hplang
{

struct TypeInfo
{
    Symbol_Type sym_type;
    const char *name;
    Type *type;
};

Type builtin_types[] = {
    //TYP_pointer,
    {TYP_null,      0, 1},
    {TYP_int_lit,   8, 8},
    {TYP_bool,      1, 1},
    {TYP_u8,        1, 1},
    {TYP_s8,        1, 1},
    {TYP_u16,       2, 2},
    {TYP_s16,       2, 2},
    {TYP_u32,       4, 4},
    {TYP_s32,       4, 4},
    {TYP_u64,       8, 8},
    {TYP_s64,       8, 8},
    {TYP_f32,       4, 4},
    {TYP_f64,       8, 8},
};

Type* GetBuiltinType(Type_Tag tt)
{
    ASSERT(tt <= TYP_f64);
    return &builtin_types[tt];
}

static TypeInfo builtin_type_infos[] = {
    //TYP_pointer,
    /*TYP_null,*/       (TypeInfo){SYM_PrimitiveType, "null_type"},
    /*TYP_int_lit,*/    (TypeInfo){SYM_PrimitiveType, "int_lit_type"},
    /*TYP_bool,*/       (TypeInfo){SYM_PrimitiveType, "bool"},
    /*TYP_u8,*/         (TypeInfo){SYM_PrimitiveType, "u8"},
    /*TYP_s8,*/         (TypeInfo){SYM_PrimitiveType, "s8"},
    /*TYP_u16,*/        (TypeInfo){SYM_PrimitiveType, "u16"},
    /*TYP_s16,*/        (TypeInfo){SYM_PrimitiveType, "s16"},
    /*TYP_u32,*/        (TypeInfo){SYM_PrimitiveType, "u32"},
    /*TYP_s32,*/        (TypeInfo){SYM_PrimitiveType, "s32"},
    /*TYP_u64,*/        (TypeInfo){SYM_PrimitiveType, "u64"},
    /*TYP_s64,*/        (TypeInfo){SYM_PrimitiveType, "s64"},
    /*TYP_f32,*/        (TypeInfo){SYM_PrimitiveType, "f32"},
    /*TYP_f64,*/        (TypeInfo){SYM_PrimitiveType, "f64"},

    /*TYP_Function,*/
    /*TYP_Struct,*/
    //TYP_Enum,
};

static void FreeScope(Scope *scope)
{
    array::Free(scope->table);
}

static void AddBuiltinTypes(Environment *env)
{
    for (s64 i = 0; i < array_length(builtin_types); i++)
    {
        Type *type = &builtin_types[i];
        const TypeInfo &info = builtin_type_infos[i];
        Name name = PushName(&env->arena, info.name);
        AddSymbol(env, info.sym_type, name, type);
    }
}

Environment NewEnvironment()
{
    Environment result = { };
    OpenScope(&result);
    AddBuiltinTypes(&result);
    return result;
}

void FreeEnvironment(Environment *env)
{
    for (s64 i = 0; i < env->scopes.count; i++)
    {
        FreeScope(array::At(env->scopes, i));
    }
    array::Free(env->scopes);
}

void OpenScope(Environment *env)
{
    Scope *scope = PushStruct<Scope>(&env->arena);
    *scope = { };
    scope->table_size = Scope::INITIAL_SYM_TABLE_SIZE;
    array::Resize(scope->table, scope->table_size);
    scope->parent = env->current;

    env->current = scope;
    array::Push(env->scopes, scope);
}

void CloseScope(Environment *env)
{
    ASSERT(env->current->parent != nullptr);
    env->current = env->current->parent;
}

Symbol* AddSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type)
{
    Symbol *symbol = PushStruct<Symbol>(&env->arena);
    *symbol = { };
    symbol->sym_type = sym_type;
    symbol->name = name;
    symbol->type = type;

    Scope *scope = env->current;
    u32 hash = name.hash % scope->table_size;
    scope->table.data[hash] = symbol;
    return symbol;
}

Symbol* LookupSymbol(Environment *env, Name name)
{
    Scope *scope = env->current;
    while (scope)
    {
        u32 hash = name.hash % scope->table_size;
        Symbol *symbol = scope->table.data[hash];
        if (symbol && symbol->name.hash == name.hash)
            return symbol;
        scope = scope->parent;
    }
    return nullptr;
}

Symbol* LookupSymbolInCurrentScope(Environment *env, Name name)
{
    Scope *scope = env->current;
    if (scope)
    {
        u32 hash = name.hash % scope->table_size;
        Symbol *symbol = scope->table.data[hash];
        if (symbol && symbol->name.hash == name.hash)
            return symbol;
    }
    return nullptr;
}

} // hplang
