
#include "error.h"
#include "assert.h"
#include "common.h"

#include <cstdio>
#include <cinttypes>

namespace hplang
{

void AddError(Error_Context *ctx, File_Location file_loc)
{
    ctx->error_count++;
    if (ctx->error_count == 1)
    {
        ctx->first_error_loc = file_loc;
    }
}

static s64 NumberLen(s64 number)
{
    s64 len = 0;
    if (number < 0)
    {
        len++;
        number = -number;
    }
    while (number > 0)
    {
        len++;
        number /= 10;
    }
    return len;
}

void PrintFileLocation(IoFile *file, File_Location file_loc)
{
    FILE *fp = (FILE*)file;

    fwrite(file_loc.file->filename.data, 1, file_loc.file->filename.size, fp);
    fprintf(fp, ":%" PRId32 ":%" PRId32 ": ", file_loc.line, file_loc.column);

    s64 loc_len = NumberLen(file_loc.line) + NumberLen(file_loc.column);
    loc_len += 2;           // add colons
    loc_len = 7 - loc_len;  // at least 7 chars long
    if (loc_len > 0)
    {
        const char spaces[] = "         ";
        if ((u64)loc_len >= sizeof(spaces))
            loc_len = sizeof(spaces) - 1;
        fwrite(spaces, 1, loc_len, fp);
    }
}

void PrintFileLine(IoFile *file, File_Location file_loc)
{
    Open_File *open_file = file_loc.file;
    ASSERT(open_file != nullptr);

    const char *file_start = (const char*)open_file->contents.ptr;
    // NOTE(henrik): The file contents of <builitin> are empty (null).
    if (!file_start) return;
    //ASSERT(file_start != nullptr);

    const char *line_start = SeekToLineStart(open_file, file_loc.offset_start);
    s64 line_len = 0;

    while (line_len < open_file->contents.size)
    {
        char c = line_start[line_len];
        if (IsNewlineChar(c))
            break;
        line_len++;
    }

    fprintf((FILE*)file, "> ");
    fwrite(line_start, 1, line_len, (FILE*)file);
    fprintf((FILE*)file, "\n");
}

void PrintFileLocArrow(IoFile *file, File_Location file_loc)
{
    const char dashes[81] = "--------------------------------------------------------------------------------";
    fprintf((FILE*)file, "> ");
    if (file_loc.column > 0 && file_loc.column < 81 - 1)
    {
        fwrite(dashes, 1, file_loc.column - 1, (FILE*)file);
        fprintf((FILE*)file, "^\n");
    }
}

void PrintTokenValue(IoFile *file, const Token *token)
{
    s64 size = token->value_end - token->value;
    FILE *f = (FILE*)file;
    for (s64 i = 0; i < size; i++)
    {
        char c = token->value[i];
        if (c == '\t')
            fputs("\\t", f);
        else if (c == '\n')
            fputs("\\n", f);
        else if (c == '\r')
            fputs("\\r", f);
        else if (c == '\f')
            fputs("\\f", f);
        else if (c == '\v')
            fputs("\\v", f);
        else
            putc(c, f);
    }
}

} // hplang
