
#include "symbols.h"
#include "assert.h"

namespace hplang
{

static void FreeScope(Scope *scope)
{
    array::Free(scope->table);
}

Environment NewEnvironment()
{
    Environment result = { };
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
