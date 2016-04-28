
#include "common.h"
#include "symbols.h"
#include "assert.h"

namespace hplang
{

b32 TypesEqual(Type *a, Type *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->tag != b->tag) return false;
    switch (a->tag)
    {
    case TYP_null:
    case TYP_int_lit:
        INVALID_CODE_PATH;
        return false;

    case TYP_pointer:
        if (a->pointer != b->pointer) return false;
        return TypesEqual(a->base_type, b->base_type);
    case TYP_bool:
    case TYP_char:
    case TYP_u8:
    case TYP_s8:
    case TYP_u16:
    case TYP_s16:
    case TYP_u32:
    case TYP_s32:
    case TYP_u64:
    case TYP_s64:
    case TYP_f32:
    case TYP_f64:
    case TYP_string:
        return true;

    case TYP_Function:
        {
            Function_Type *ft_a = &a->function_type;
            Function_Type *ft_b = &b->function_type;
            if (!TypesEqual(ft_a->return_type, ft_b->return_type))
            {
                return false;
            }
            if (ft_a->parameter_count != ft_b->parameter_count)
            {
                return false;
            }
            for (s64 i = 0; i < ft_a->parameter_count; i++)
            {
                if (!TypesEqual(
                        ft_a->parameter_types[i],
                        ft_b->parameter_types[i]))
                {
                    return false;
                }
            }
            return true;
        }
    case TYP_Struct:
        {
            // NOTE(henrik): This is not reached as every struct type is unique
            // and will be catched by the check by identity.
            INVALID_CODE_PATH;
            return false;
        }
    }
    INVALID_CODE_PATH;
    return false;
}

struct TypeInfo
{
    Symbol_Type sym_type;
    const char *name;
    Type *type;
};

Type builtin_types[] = {
    {TYP_null,      0, 1},
    {TYP_int_lit,   8, 8},
    {TYP_pointer,   8, 8},
    {TYP_bool,      1, 1},
    {TYP_char,      1, 1},
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
    {TYP_string,    16, 8},
};

Type* GetBuiltinType(Type_Tag tt)
{
    ASSERT(tt <= TYP_LAST_BUILTIN);
    return &builtin_types[tt];
}

static TypeInfo builtin_type_infos[] = {
    /*TYP_null,*/       (TypeInfo){SYM_PrimitiveType,   "null_type"},
    /*TYP_int_lit,*/    (TypeInfo){SYM_PrimitiveType,   "int_lit_type"},
    /*TYP_pointer,*/    (TypeInfo){SYM_PrimitiveType,   "pointer_type"},
    /*TYP_bool,*/       (TypeInfo){SYM_PrimitiveType,   "bool"},
    /*TYP_char,*/       (TypeInfo){SYM_PrimitiveType,   "char"},
    /*TYP_u8,*/         (TypeInfo){SYM_PrimitiveType,   "u8"},
    /*TYP_s8,*/         (TypeInfo){SYM_PrimitiveType,   "s8"},
    /*TYP_u16,*/        (TypeInfo){SYM_PrimitiveType,   "u16"},
    /*TYP_s16,*/        (TypeInfo){SYM_PrimitiveType,   "s16"},
    /*TYP_u32,*/        (TypeInfo){SYM_PrimitiveType,   "u32"},
    /*TYP_s32,*/        (TypeInfo){SYM_PrimitiveType,   "s32"},
    /*TYP_u64,*/        (TypeInfo){SYM_PrimitiveType,   "u64"},
    /*TYP_s64,*/        (TypeInfo){SYM_PrimitiveType,   "s64"},
    /*TYP_f32,*/        (TypeInfo){SYM_PrimitiveType,   "f32"},
    /*TYP_f64,*/        (TypeInfo){SYM_PrimitiveType,   "f64"},
    /*TYP_string,*/     (TypeInfo){SYM_Struct,          "string"},

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
    ASSERT(array_length(builtin_types) == TYP_LAST_BUILTIN + 1);
    for (s64 i = 0;
             i < array_length(builtin_types);
             i++)
    {
        Type *type = &builtin_types[i];
        const TypeInfo &info = builtin_type_infos[i];
        Name name = PushName(&env->arena, info.name);
        if (info.sym_type == SYM_PrimitiveType)
            type->type_name = name;
        else if (info.sym_type == SYM_Struct)
            type->struct_type.name = name;

        if (i >= TYP_FIRST_BUILTIN_SYM)
            AddSymbol(env, info.sym_type, name, type);
    }
    Type *string_type = GetBuiltinType(TYP_string);
    Struct_Member *members = PushArray<Struct_Member>(&env->arena, 2);
    string_type->struct_type.member_count = 2;
    string_type->struct_type.members = members;

    members[0].name = PushName(&env->arena, "size");
    members[0].type = GetBuiltinType(TYP_s64);
    members[1].name = PushName(&env->arena, "data");
    Type *data_ptr = PushType(env, TYP_pointer);
    data_ptr->pointer = 1;
    data_ptr->base_type = GetBuiltinType(TYP_char);
    members[1].type = data_ptr;
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

//static const s64 INITIAL_SYM_TABLE_SIZE = 1021; // Prime number
static const s64 INITIAL_SYM_TABLE_SIZE = 127; // Prime number

void OpenScope(Environment *env)
{
    Scope *scope = PushStruct<Scope>(&env->arena);
    *scope = { };
    array::Resize(scope->table, INITIAL_SYM_TABLE_SIZE);
    scope->parent = env->current;
    if (env->current)
    {
        scope->return_type = env->current->return_type;
    }

    env->current = scope;
    array::Push(env->scopes, scope);
}

void CloseScope(Environment *env)
{
    ASSERT(env->current->parent != nullptr);
    env->current = env->current->parent;
    env->return_type = env->current->return_type;
}

void OpenFunctionScope(Environment *env, Type *return_type)
{
    OpenScope(env);
    env->current->return_type = return_type;
}

void CloseFunctionScope(Environment *env)
{
    CloseScope(env);
}

Type* PushType(Environment *env, Type_Tag tag)
{
    Type *type = PushStruct<Type>(&env->arena);
    *type = { };
    type->tag = tag;
    return type;
}

static void PutHash(Array<Symbol*> &arr, Name name, Symbol *symbol);

static void GrowTable(Array<Symbol*> &arr)
{
    s64 table_size = arr.count;
    s64 new_table_size = table_size + INITIAL_SYM_TABLE_SIZE;

    Array<Symbol*> temp = { };
    array::Resize(temp, new_table_size);

    for (s64 i = 0; i < table_size; i++)
    {
        Symbol *symbol = array::At(arr, i);
        PutHash(temp, symbol->name, symbol);
    }

    array::Free(arr);
    arr = temp;
}

static void PutHash(Array<Symbol*> &arr, Name name, Symbol *symbol)
{
    u32 tries = 0;
    while (tries < 2)
    {
        u32 table_size = arr.count;
        u32 index = name.hash % table_size;
        u32 probe_offset = 0;
        Symbol *old_symbol = array::At(arr, index);
        while (old_symbol && probe_offset < table_size)
        {
            probe_offset++;
            u32 i = (index + probe_offset) % table_size;
            old_symbol = array::At(arr, i);
        }
        if (old_symbol)
        {
            GrowTable(arr);
        }
        else
        {
            u32 i = (index + probe_offset) % table_size;
            arr.data[i] = symbol;
            return;
        }
        tries++;
    }
    // NOTE(henrik): The symbol should have been inserted after
    // growing the table, but apparently that did not happen.
    INVALID_CODE_PATH;
}

static Symbol* LookupSymbol(Scope *scope, Name name)
{
    u32 table_size = scope->table.count;
    u32 hash = name.hash % table_size;
    u32 probe_offset = 0;
    Symbol *symbol = scope->table.data[hash];
    while (symbol && probe_offset < table_size)
    {
        if (symbol && symbol->name.hash == name.hash)
            return symbol;
        probe_offset++;
        symbol = scope->table.data[(hash + probe_offset) % table_size];
    }
    return nullptr;
}

static Symbol* PushSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type)
{
    Symbol *symbol = PushStruct<Symbol>(&env->arena);
    *symbol = { };
    symbol->sym_type = sym_type;
    symbol->name = name;
    symbol->type = type;
    return symbol;
}

Symbol* AddSymbol(Environment *env, Symbol_Type sym_type, Name name, Type *type)
{
    Symbol *symbol = PushSymbol(env, sym_type, name, type);

    Scope *scope = env->current;
    PutHash(scope->table, name, symbol);
    scope->symbol_count++;
    return symbol;
}

Symbol* AddFunction(Environment *env, Name name, Type *type)
{
    Scope *scope = env->current;
    Symbol *old_symbol = LookupSymbol(scope, name);
    if (old_symbol)
    {
        if (old_symbol->sym_type == SYM_Function)
        {
            Symbol *symbol = PushSymbol(env, SYM_Function, name, type);
            Symbol *prev = old_symbol;
            while (prev->next_overload)
            {
                prev = prev->next_overload;
            }
            prev->next_overload = symbol;
            return symbol;
        }
        return old_symbol;
    }
    else
    {
        Symbol *symbol = PushSymbol(env, SYM_Function, name, type);
        PutHash(scope->table, name, symbol);
        scope->symbol_count++;
        return symbol;
    }
}

Symbol* LookupSymbol(Environment *env, Name name)
{
    Scope *scope = env->current;
    while (scope)
    {
        Symbol *sym = LookupSymbol(scope, name);
        if (sym) return sym;
        scope = scope->parent;
    }
    return nullptr;
}

Symbol* LookupSymbolInCurrentScope(Environment *env, Name name)
{
    Scope *scope = env->current;
    ASSERT(scope != nullptr);
    return LookupSymbol(scope, name);
}

} // hplang
