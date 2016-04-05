
#include "token.h"

namespace hplang
{

Token_Arena* AllocateTokenArena(Token_Arena *arena, s64 token_count)
{
    s64 needed_size = sizeof(Token_Arena) + token_count * sizeof(Token);
    Pointer pointer = Alloc(needed_size);
    if (!pointer.ptr)
        return arena;

    Token_Arena *new_arena = (Token_Arena*)pointer.ptr;
    Token *tokens_begin = (Token*)(new_arena + 1);
    new_arena->memory = pointer;
    new_arena->begin = tokens_begin;
    new_arena->end = new_arena->begin + token_count;
    new_arena->current = new_arena->begin;
    new_arena->prev_arena = arena;

    return new_arena;
}

void DeallocateTokenArena(Token_Arena *arena)
{
    do
    {
        Token_Arena *prev_arena = arena->prev_arena;
        Free(arena->memory);
        arena = prev_arena;
    } while (arena);
}

Token* PushToken(Token_Arena *arena)
{
    if (arena->current >= arena->end)
    {
        const s64 token_count = DEFAULT_TOKEN_ARENA_SIZE;
        arena = AllocateTokenArena(arena, token_count);
        if (arena->current >= arena->end)
            return nullptr;
    }
    Token tok = { };
    Token *token = arena->current++;
    *token = tok;
    return token;
}

} // hplang
