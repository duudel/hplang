
#include "common.h"
#include "symbols.h"
#include "assert.h"

#include <cstdio>
#include <cinttypes>

namespace hplang
{

struct Type_Info
{
    Type_Tag tag;
    Symbol_Type sym_type;
    u32 size;
    u32 alignment;
    const char *name;
};

static const Type_Info builtin_type_infos[] = {
    {TYP_none,      SYM_PrimitiveType, 0, 1,  "none_type"},
    {TYP_pending,   SYM_PrimitiveType, 0, 1,  "pending_type"},
    {TYP_null,      SYM_PrimitiveType, 8, 8,  "null_type"},
    {TYP_pointer,   SYM_PrimitiveType, 8, 8,  "pointer_type"},

    {TYP_void,      SYM_PrimitiveType, 0, 1,  "void"},
    {TYP_bool,      SYM_PrimitiveType, 1, 1,  "bool"},
    {TYP_char,      SYM_PrimitiveType, 1, 1,  "char"},
    {TYP_u8,        SYM_PrimitiveType, 1, 1,  "u8"},
    {TYP_s8,        SYM_PrimitiveType, 1, 1,  "s8"},
    {TYP_u16,       SYM_PrimitiveType, 2, 2,  "u16"},
    {TYP_s16,       SYM_PrimitiveType, 2, 2,  "s16"},
    {TYP_u32,       SYM_PrimitiveType, 4, 4,  "u32"},
    {TYP_s32,       SYM_PrimitiveType, 4, 4,  "s32"},
    {TYP_u64,       SYM_PrimitiveType, 8, 8,  "u64"},
    {TYP_s64,       SYM_PrimitiveType, 8, 8,  "s64"},
    {TYP_f32,       SYM_PrimitiveType, 4, 4,  "f32"},
    {TYP_f64,       SYM_PrimitiveType, 8, 8,  "f64"},
    {TYP_string,    SYM_Struct,        0, 0,  "string"},

    /*TYP_Function,*/
    /*TYP_Struct,*/
    //TYP_Enum,
};

b32 TypeIsNone(Type *t)
{
    if (!t) return false;
    return t->tag == TYP_none;
}

b32 TypeIsPending(Type *t)
{
    if (!t) return false;
    return t->tag == TYP_pending;
}

b32 TypeIsNull(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsNull(t->base_type);
    return t->tag == TYP_null;
}

b32 TypeIsPointer(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsPointer(t->base_type);
    return (t->tag == TYP_pointer) || (t->tag == TYP_null);
}

b32 TypeIsVoid(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsVoid(t->base_type);
    return t->tag == TYP_void;
}

b32 TypeIsBoolean(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsBoolean(t->base_type);
    return t->tag == TYP_bool;
}

b32 TypeIsChar(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsChar(t->base_type);
    return t->tag == TYP_char;
}

b32 TypeIsIntegral(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsIntegral(t->base_type);
    switch (t->tag)
    {
        case TYP_u8: case TYP_s8:
        case TYP_u16: case TYP_s16:
        case TYP_u32: case TYP_s32:
        case TYP_u64: case TYP_s64:
            return true;
        default:
            break;
    }
    return false;
}

b32 TypeIsFloat(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsFloat(t->base_type);
    return (t->tag == TYP_f32) || (t->tag == TYP_f64);
}

b32 TypeIsNumeric(Type *t)
{
    return TypeIsIntegral(t) || TypeIsFloat(t);
}

b32 TypeIsString(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsString(t->base_type);
    return t->tag == TYP_string;
}

b32 TypeIsStruct(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsStruct(t->base_type);
    return t->tag == TYP_Struct || t->tag == TYP_string;
}

b32 TypeIsSigned(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsSigned(t->base_type);
    switch (t->tag)
    {
        case TYP_s8:
        case TYP_s16:
        case TYP_s32:
        case TYP_s64:
            return true;
        default:
            return false;
    }
    return false;
}

b32 TypeIsUnsigned(Type *t)
{
    if (!t) return false;
    if (TypeIsPending(t)) return TypeIsUnsigned(t->base_type);
    switch (t->tag)
    {
        case TYP_u8:
        case TYP_u16:
        case TYP_u32:
        case TYP_u64:
            return true;
        default:
            return false;
    }
    return false;
}

b32 TypesEqual(Type *a, Type *b)
{
    if (a == b) return true;
    if (TypeIsPending(a)) a = a->base_type;
    if (TypeIsPending(b)) b = b->base_type;

    if (a->tag != b->tag) return false;
    switch (a->tag)
    {
    case TYP_none:
        return false;
    case TYP_pending:
    case TYP_null:
        INVALID_CODE_PATH;
        return false;

    case TYP_pointer:
        if (!TypeIsPointer(b)) return false;
        return TypesEqual(a->base_type, b->base_type);
    case TYP_void:
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

void ResolvePhysicalTypeInfo(Type *type)
{
    if (type->alignment == 0)
    {
        if (type->tag == TYP_pointer)
        {
            type->size = 8;
            type->alignment = 8;
        }
        else if (type->tag == TYP_Function)
        {
            type->size = 8;
            type->alignment = 8;
        }
        else if (TypeIsStruct(type))
        {
            u32 size = 0;
            u32 align = 0;
            for (s64 i = 0; i < type->struct_type.member_count; i++)
            {
                Struct_Member *member = &type->struct_type.members[i];
                ResolvePhysicalTypeInfo(member->type);

                u32 member_align = member->type->alignment;
                size = Align(size, member_align);
                align = align > member_align ? align : member_align;

                member->offset = size;
                size += member->type->size;
            }
            type->size = size;
            type->alignment = align;
        }
        else
        {
            INVALID_CODE_PATH;
        }
    }
}

s64 GetStructMemberOffset(Type *type, s64 member_index)
{
    if (TypeIsStruct(type))
    {
        ASSERT(member_index < type->struct_type.member_count);
        ResolvePhysicalTypeInfo(type);
        return type->struct_type.members[member_index].offset;
    }
    INVALID_CODE_PATH;
    return 0;
}

u32 GetSize(Type *type)
{
    ResolvePhysicalTypeInfo(type);
    return type->size;
}

u32 GetAlign(Type *type)
{
    ResolvePhysicalTypeInfo(type);
    return type->alignment;
}

u32 GetAlignedSize(Type *type)
{
    ResolvePhysicalTypeInfo(type);
    return Align(type->size, type->alignment);
}

u32 GetAlignedElementSize(Type *type)
{
    ASSERT(TypeIsPointer(type) /* || TypeIsArray(type)*/);
    return GetAlignedSize(type->base_type);
}

Type* GetBuiltinType(Environment *env, Type_Tag tag)
{
    ASSERT(tag <= TYP_LAST_BUILTIN);
    return env->builtin_types[tag];
}

Type* GetPointerType(Environment *env, Type *base_type)
{
    Type *pointer_type = base_type->pointer_type;
    if (!pointer_type)
    {
        pointer_type = PushType(env, TYP_pointer);
        pointer_type->base_type = base_type;
        base_type->pointer_type = pointer_type;
    }
    return pointer_type;
}


s64 PrintFunctionType(IoFile *file, Type *return_type, s64 param_count, Type **param_types)
{
    s64 n = fprintf((FILE*)file, "(");
    for (s64 i = 0; i < param_count; i++)
    {
        if (i > 0) n += fprintf((FILE*)file, ", ");
        n += PrintType(file, param_types[i]);
    }
    n += fprintf((FILE*)file, ")");
    n += fprintf((FILE*)file, " : ");
    if (return_type)
        n += PrintType(file, return_type);
    else
        n += fprintf((FILE*)file, "?");
    return n;
}

s64 PrintType(IoFile *file, Type *type)
{
    switch (type->tag)
    {
    case TYP_pending:
        //fprintf((FILE*)file, "(?");
        if (type->base_type)
            return PrintType(file, type->base_type);
        //fprintf((FILE*)file, ")");
        break;
    case TYP_string:
    case TYP_Struct:
        return PrintString(file, type->struct_type.name.str);
    case TYP_Function:
        {
            return PrintFunctionType(file, type->function_type.return_type,
                    type->function_type.parameter_count,
                    type->function_type.parameter_types);
        } break;
    case TYP_pointer:
        {
            s64 n = PrintType(file, type->base_type);
            return n + fprintf((FILE*)file, "*");
        } break;

    // NOTE(henrik): Even thoug this should not appear in any normal case,
    // this could still happen, when function overload resolution fails.
    case TYP_null:
        return fprintf((FILE*)file, "(?*)null");
        break;

    default:
        if (type->tag <= TYP_LAST_BUILTIN)
        {
            return PrintString(file, type->type_name.str);
        }
    }
    INVALID_CODE_PATH;
    return 0;
}

b32 SymbolIsGlobal(Symbol *symbol)
{
    return (symbol->flags & SYMF_Global) != 0;
}

b32 SymbolIsIntrinsic(Symbol *symbol)
{
    return (symbol->flags & SYMF_Intrinsic) != 0;
}

static void SetBuiltinFileLocation(Environment *env)
{
    File_Location file_loc = { };
    file_loc.file = PushStruct<Open_File>(&env->arena);
    file_loc.file->filename = PushString(&env->arena, "<builtin>");
    env->builtin_file_loc = file_loc;
}

static void AddBuiltinTypes(Environment *env)
{
    ASSERT(array_length(builtin_type_infos) == TYP_LAST_BUILTIN + 1);
    for (s64 i = 0;
             i < array_length(builtin_type_infos);
             i++)
    {
        const Type_Info &type_info = builtin_type_infos[i];
        ASSERT(type_info.tag == i);

        Name name = PushName(&env->arena, type_info.name);

        Type *type = PushType(env, type_info.tag);
        type->size = type_info.size;
        type->alignment = type_info.alignment;
        type->pointer_type = nullptr;

        if (type_info.sym_type == SYM_PrimitiveType)
            type->type_name = name;
        else if (type_info.sym_type == SYM_Struct)
            type->struct_type.name = name;

        env->builtin_types[type_info.tag] = type;

        if (type_info.tag >= TYP_FIRST_BUILTIN_SYM)
            AddSymbol(env, type_info.sym_type, name, type, env->builtin_file_loc);
    }
    Type *string_type = GetBuiltinType(env, TYP_string);
    Struct_Member *members = PushArray<Struct_Member>(&env->arena, 2);
    string_type->struct_type.member_count = 2;
    string_type->struct_type.members = members;

    members[0].name = PushName(&env->arena, "size");
    members[0].type = GetBuiltinType(env, TYP_s64);
    members[0].offset = 0;
    members[1].name = PushName(&env->arena, "data");
    members[1].type = GetPointerType(env, GetBuiltinType(env, TYP_char));
    members[1].offset = 8;
}

static void AddBuiltinFunctions(Environment *env)
{
    Type *void_type = GetBuiltinType(env, TYP_void);

    Type *hp_alloc_type = PushFunctionType(env, TYP_Function, 1);
    hp_alloc_type->function_type.return_type = GetPointerType(env, void_type);
    hp_alloc_type->function_type.parameter_types[0] = GetBuiltinType(env, TYP_s64);
    AddSymbol(env, SYM_ForeignFunction, PushName(&env->arena, "alloc"), hp_alloc_type, env->builtin_file_loc);

    Type *hp_free_type = PushFunctionType(env, TYP_Function, 1);
    hp_free_type->function_type.return_type = void_type;
    hp_free_type->function_type.parameter_types[0] = GetPointerType(env, void_type);
    AddSymbol(env, SYM_ForeignFunction, PushName(&env->arena, "free"), hp_free_type, env->builtin_file_loc);

    Type *c_exit_type = PushFunctionType(env, TYP_Function, 1);
    c_exit_type->function_type.return_type = void_type;
    c_exit_type->function_type.parameter_types[0] = GetBuiltinType(env, TYP_s32);
    AddSymbol(env, SYM_ForeignFunction, PushName(&env->arena, "exit"), c_exit_type, env->builtin_file_loc);

    Name sqrt_name = PushName(&env->arena, "sqrt");
    Type *sqrt_f64_type = PushFunctionType(env, TYP_Function, 1);
    sqrt_f64_type->function_type.return_type = GetBuiltinType(env, TYP_f64);
    sqrt_f64_type->function_type.parameter_types[0] = GetBuiltinType(env, TYP_f64);
    Symbol *sqrt_sym = AddFunction(env, sqrt_name, sqrt_f64_type, NoFileLocation());
    sqrt_sym->flags = SYMF_Intrinsic;
}

Environment NewEnvironment(const char *main_func_name)
{
    Environment result = { };
    OpenScope(&result);
    result.root = result.current;

    SetBuiltinFileLocation(&result);

    AddBuiltinTypes(&result);
    AddBuiltinFunctions(&result);
    result.main_func_name = PushName(&result.arena, main_func_name);
    return result;
}

static void FreeScope(Scope *scope)
{
    array::Free(scope->table);
}

void FreeEnvironment(Environment *env)
{
    for (s64 i = 0; i < env->scopes.count; i++)
    {
        FreeScope(array::At(env->scopes, i));
    }
    array::Free(env->scopes);
    FreeMemoryArena(&env->arena);
}

Scope* CurrentScope(Environment *env)
{
    return env->current;
}

void SetCurrentScope(Environment *env, Scope *scope)
{
    env->current = scope;
}

//static const s64 INITIAL_SYM_TABLE_SIZE = 1021; // Prime number
static const s64 INITIAL_SYM_TABLE_SIZE = 127; // Prime number

void OpenScope(Environment *env)
{
    Scope *scope = PushStruct<Scope>(&env->arena);
    *scope = { };
    array::Resize(scope->table, INITIAL_SYM_TABLE_SIZE);

    scope->scope_id = env->next_scope_id++;
    scope->parent = env->current;
    if (env->current)
    {
        scope->return_type = scope->parent->return_type;
        scope->rt_infer_loc = scope->parent->rt_infer_loc;
    }

    env->current = scope;
    array::Push(env->scopes, scope);
}

void CloseScope(Environment *env)
{
    ASSERT(env->current->parent != nullptr);
    Type *return_type = env->current->return_type;
    Ast_Node *rt_infer_loc = env->current->rt_infer_loc;
    s64 return_stmts = env->current->return_stmt_count;

    env->current = env->current->parent;
    if (return_type)
    {
        env->current->return_type = return_type;
        env->current->rt_infer_loc = rt_infer_loc;
    }
    env->current->return_stmt_count = return_stmts;
}

void OpenFunctionScope(Environment *env, Name scope_name, Type *return_type)
{
    OpenScope(env);
    env->current->scope_name = scope_name;
    env->current->return_type = return_type;
    env->current->rt_infer_loc = nullptr;
}

Type* CloseFunctionScope(Environment *env)
{
    ASSERT(env->current->parent != nullptr);
    Type *return_type = env->current->return_type;
    if (TypeIsPending(return_type))
    {
        // NOTE(henrik): Infer return type to be void, if no return statements
        // were encountered.
        if (env->current->return_stmt_count == 0)
        {
            InferReturnType(env, GetBuiltinType(env, TYP_void), nullptr);
        }
    }

    env->current = env->current->parent;

    return return_type;
}

void IncReturnStatements(Environment *env)
{ env->current->return_stmt_count++; }

s64 GetReturnStatements(Environment *env)
{ return env->current->return_stmt_count; }

Type* GetCurrentReturnType(Environment *env)
{ return env->current->return_type; }

Ast_Node* GetCurrentReturnTypeInferLoc(Environment *env)
{ return env->current->rt_infer_loc; }

void InferReturnType(Environment *env, Type *return_type, Ast_Node *location)
{
    ASSERT(TypeIsPending(env->current->return_type));
    env->current->return_type->base_type = return_type;
    env->current->rt_infer_loc = location;
}


Type* PushType(Environment *env, Type_Tag tag)
{
    Type *type = PushStruct<Type>(&env->arena);
    *type = { };
    type->tag = tag;
    return type;
}

Type* PushPendingType(Environment *env)
{
    return PushType(env, TYP_pending);
}

Type* PushFunctionType(Environment *env, Type_Tag tag, s64 param_count)
{
    Type *ftype = PushType(env, tag);
    ftype->function_type.parameter_count = param_count;
    if (param_count > 0)
    {
        ftype->function_type.parameter_types = PushArray<Type*>(&env->arena, param_count);
        for (s64 i = 0; i < param_count; i++)
            ftype->function_type.parameter_types[i] = nullptr;
    }
    else
        ftype->function_type.parameter_types = nullptr;
    return ftype;
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
        if (symbol && symbol->name == name)
            return symbol;
        probe_offset++;
        symbol = scope->table.data[(hash + probe_offset) % table_size];
    }
    return nullptr;
}

static Name MakeUniqueName(Environment *env, Name name)
{
    s64 size = 0;
    for (Scope *scope = env->current; scope; scope = scope->parent)
    {
        if (scope->scope_name.str.size != 0)
        {
            size += scope->scope_name.str.size;
            size += 1;
        }
        else if (scope->scope_id != 0)
        {
            size += snprintf(nullptr, 0, "@%" PRId64, scope->scope_id);
        }
    }
    size += name.str.size;

    String str = { };
    str.size = size;
    str.data = PushArray<char>(&env->arena, size);

    s64 pos = 0;
    for (Scope *scope = env->current; scope != nullptr; scope = scope->parent)
    {
        if (scope->scope_name.str.size != 0)
        {
            for (s64 i = 0; i < scope->scope_name.str.size; i++, pos++)
            {
                str.data[pos] = scope->scope_name.str.data[i];
            }
            str.data[pos++] = '@';
        }
        else if (scope->scope_id != 0)
        {
            pos += snprintf(&str.data[pos], size - pos, "@%" PRId64, scope->scope_id);
        }
    }
    for (s64 i = 0; i < name.str.size; i++, pos++)
    {
        str.data[pos] = name.str.data[i];
    }

    return MakeName(str);
}

static Symbol* PushSymbol(Environment *env,
        Symbol_Type sym_type, Name name, Type *type, File_Location define_loc)
{
    Symbol *symbol = PushStruct<Symbol>(&env->arena);
    *symbol = { };
    symbol->sym_type = sym_type;
    symbol->name = name;
    symbol->unique_name = MakeUniqueName(env, name);
    symbol->type = type;
    symbol->define_loc = define_loc;
    return symbol;
}

Symbol* AddSymbol(Environment *env,
        Symbol_Type sym_type, Name name, Type *type, File_Location define_loc)
{
    Symbol *symbol = PushSymbol(env, sym_type, name, type, define_loc);

    Scope *scope = env->current;
    PutHash(scope->table, name, symbol);
    scope->symbol_count++;
    return symbol;
}

static s64 UniqueTypeString(char *buf, s64 bufsize, Type *type)
{
    switch (type->tag)
    {
        case TYP_none: INVALID_CODE_PATH; break;
        case TYP_null: INVALID_CODE_PATH; break;
        case TYP_pending:
            return UniqueTypeString(buf, bufsize, type->base_type);
        case TYP_pointer:
            {
                s64 len = snprintf(buf, bufsize, "P");
                return len + UniqueTypeString(buf + len, bufsize - len, type->base_type);
            }
        case TYP_void:
            return snprintf(buf, bufsize, "v");
        case TYP_bool:
            return snprintf(buf, bufsize, "b");
        case TYP_char:
            return snprintf(buf, bufsize, "c");
        case TYP_u8:
            return snprintf(buf, bufsize, "u1");
        case TYP_s8:
            return snprintf(buf, bufsize, "s1");
        case TYP_u16:
            return snprintf(buf, bufsize, "u2");
        case TYP_s16:
            return snprintf(buf, bufsize, "s2");
        case TYP_u32:
            return snprintf(buf, bufsize, "u4");
        case TYP_s32:
            return snprintf(buf, bufsize, "s4");
        case TYP_u64:
            return snprintf(buf, bufsize, "u8");
        case TYP_s64:
            return snprintf(buf, bufsize, "s8");
        case TYP_f32:
            return snprintf(buf, bufsize, "f4");
        case TYP_f64:
            return snprintf(buf, bufsize, "f8");
        case TYP_string:
            return snprintf(buf, bufsize, "S");
        case TYP_Function:
            {
                s64 len = snprintf(buf, bufsize, "#");
                len += UniqueTypeString(buf + len, bufsize - len, type->function_type.return_type);
                len += snprintf(buf + len, bufsize - len, "$");
                for (s64 i = 0; i < type->function_type.parameter_count; i++)
                {
                    Type *param_type = type->function_type.parameter_types[i];
                    len += UniqueTypeString(buf + len, bufsize - len, param_type);
                    len += snprintf(buf + len, bufsize - len, "$");
                }
                return len;
            }
        case TYP_Struct:
            {
                Name struct_name = type->struct_type.name;
                s64 len = snprintf(buf, bufsize, "T");

                if (len + struct_name.str.size < bufsize)
                {
                    for (s64 i = 0; i < struct_name.str.size; i++)
                    {
                        buf[len + i] = struct_name.str.data[i];
                    }
                }
                return len + struct_name.str.size;
            }
        default:
            break;
    }
    INVALID_CODE_PATH;
    return 0;
}

static Name MakeUniqueOverloadName(Environment *env, Name base_name, Type *type)
{
    if (base_name == env->main_func_name)
    {
        return base_name;
    }

    ASSERT(type->tag == TYP_Function);
    s64 buf_size = 32;
    s64 allocated_size = base_name.str.size + buf_size;
    char *buf = PushArray<char>(&env->arena, allocated_size);
    s64 len = UniqueTypeString(buf + base_name.str.size, buf_size, type);

    if (len >= buf_size)
    {
        TryToFreeData(&env->arena, buf, allocated_size);
        buf_size = len + 1;
        buf = PushArray<char>(&env->arena, base_name.str.size + buf_size);
        len = UniqueTypeString(buf + base_name.str.size, buf_size, type);
    }

    String str;
    str.size = len + base_name.str.size;
    str.data = buf;

    for (s64 i = 0; i < base_name.str.size; i++)
    {
        str.data[i] = base_name.str.data[i];
    }

    return MakeName(str);
}

Symbol* AddFunction(Environment *env, Name name, Type *type, File_Location define_loc)
{
    Scope *scope = env->current;
    Symbol *old_symbol = LookupSymbol(scope, name);
    if (old_symbol)
    {
        if (old_symbol->sym_type == SYM_Function)
        {
            Symbol *symbol = PushSymbol(env, SYM_Function, name, type, define_loc);
            Symbol *prev = old_symbol;
            while (prev->next_overload)
            {
                prev = prev->next_overload;
            }
            prev->next_overload = symbol;
            scope->symbol_count++;
            return symbol;
        }
        return old_symbol;
    }
    else
    {
        Symbol *symbol = PushSymbol(env, SYM_Function, name, type, define_loc);
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

void ResolveTypeInformation(Environment *env)
{
    for (s64 i = 0; i < env->root->table.count; i++)
    {
        Symbol *symbol = env->root->table[i];
        if (!symbol) continue;

        if (symbol->sym_type == SYM_ForeignFunction ||
            symbol->sym_type == SYM_Function)
        {
            do {
                symbol->unique_name = MakeUniqueOverloadName(env, symbol->name, symbol->type);
                symbol = symbol->next_overload;
            } while (symbol);
        }
    }
}

} // hplang
