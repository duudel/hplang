#ifndef H_HPLANG_SYMBOLS_H

namespace // hplang
{

enum Symbol_Type
{
    SYM_Module,
    SYM_Struct,
    //SYM_Enum,
    SYM_Function,
    SYM_Variable,
    SYM_Parameter,
    SYM_Member,
    //SYM_Typealias
};

struct Symbol
{
    Symbol_Type type;
    Name name;
};

struct Scope
{
    static const s64 INITIAL_SYM_TABLE_SIZE = 1021; // Prime number

    s64 table_size;
    Symbol **table;

    Scope *parent;
};

// TODO(henrik): Is there better name for this?
struct Environment
{
    Memory_Arena arena;
    Scope *root; // The global scope of the root module
};

Scope* OpenScope(Environment *env, Scope *scope);
Scope* CloseScope(Environment *env, Scope *scope);

Symbol* ScopeAddSymbol(Environment *env, Symbol_Type type, Name name);

} // hplang

#define H_HPLANG_SYMBOLS_H
#endif
