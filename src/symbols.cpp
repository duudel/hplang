
#include "symbols.h"
#include "assert.h"

namespace hplang
{

void FreeScope(Scope scope)
{
    array::Free(scope.table);
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
    scope->parent = env->current;

    env->current = scope;
    array::Push(env->scopes, scope);
}

void CloseScope(Environment *env)
{
    ASSERT(scope->parent != nullptr);
    return scope->parent;
}

Symbol* AddSymbol(Environment *env, Symbol_Type type, Name name)
{
    Symbol *symbol = PushStruct<Symbol>(&env->arena);
    *symbol = { };
    symbol->type = type;
    symbol->name = name;
    return symbol;
}

Symbol* LookupSymbol(Environment *env, Name name)
{
    return nullptr;
    Scope *scope = env->current;
    while (scope)
    {
        u32 hash = name.hash % scope->table_size;
        Symbol *sym = scope->table[hash];
    }
}

} // hplang
