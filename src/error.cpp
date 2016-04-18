
#include "error.h"

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

void PrintFileLocation(FILE *file, File_Location file_loc)
{
    fwrite(file_loc.filename.data, 1, file_loc.filename.size, file);
    fprintf(file, ":%lld:%lld: ", file_loc.line, file_loc.column);

    s64 loc_len = NumberLen(file_loc.line) + NumberLen(file_loc.column);
    loc_len += 2;           // add colons
    loc_len = 7 - loc_len;  // at least 7 chars long
    if (loc_len > 0)
    {
        const char spaces[] = "         ";
        if ((u64)loc_len >= sizeof(spaces))
            loc_len = sizeof(spaces) - 1;
        fwrite(spaces, 1, loc_len, file);
    }
}

void PrintTokenValue(FILE *file, const Token *token)
{
    // TODO(henrik): There is a problem, when the token value contains newline
    // characters. Should loop through and transform to escape sequences.
    s64 size = token->value_end - token->value;
    fwrite(token->value, 1, size, file);
}

} // hplang
