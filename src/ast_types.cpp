
#include "ast_types.h"
#include "memory.h"

namespace hplang
{

static b32 GrowNodeList(Ast_Node_List *nodes)
{
    s64 new_capacity = 0;
    if (nodes->capacity < 2)
        new_capacity = 8;
    else
        new_capacity = nodes->capacity + nodes->capacity / 2;
    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node);
    nodes_p = Realloc(nodes_p, new_capacity);
    if (nodes_p.ptr)
    {

        return true;
    }
    return false;
}

void FreeNodeList(Ast_Node_List *nodes)
{
    Pointer nodes_p;
    nodes_p.ptr = nodes->nodes;
    nodes_p.size = nodes->capacity * sizeof(Ast_Node);
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

} // hplang
