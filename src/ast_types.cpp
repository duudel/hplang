
#include "ast_types.h"
#include "token.h"
#include "memory.h"
#include "assert.h"

namespace hplang
{

static b32 GrowNodeList(Ast_Node_List *nodes)
{
    ASSERT(nodes->capacity >= 0);
    s64 old_capacity = nodes->capacity;
    s64 new_capacity = old_capacity + old_capacity / 2;
    if (new_capacity < 8) new_capacity = 8;

    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node*);
    nodes_p = Realloc(nodes_p, new_capacity * sizeof(Ast_Node*));
    if (nodes_p.ptr)
    {
        nodes->capacity = new_capacity;
        nodes->nodes = (Ast_Node**)nodes_p.ptr;
        return true;
    }
    ASSERT(0 && "SHOULD NOT HAPPEN IN NORMAL USE");
    return false;
}

void FreeNodeList(Ast_Node_List *nodes)
{
    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node*);
    Free(nodes_p);
}

b32 PushNodeList(Ast_Node_List *nodes, Ast_Node *node)
{
    if (nodes->node_count >= nodes->capacity)
    {
        if (!GrowNodeList(nodes))
            return false;
    }
    nodes->nodes[nodes->node_count++] = node;
    return true;
}

// AST
// ---

Ast_Node* PushAstNode(Ast *ast, Ast_Node_Type node_type, const Token *token)
{
    Ast_Node *node = PushStruct<Ast_Node>(&ast->arena);
    *node = { };
    node->type = node_type;
    node->token = token;
    return node;
}

void FreeAst(Ast *ast)
{
    FreeMemoryArena(&ast->arena);
    ast->root = nullptr;
    if (ast->tokens)
    {
        FreeTokenList(ast->tokens);
        ast->tokens = nullptr;
    }
}

} // hplang
