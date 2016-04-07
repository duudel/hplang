
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

void PrintFileLocation(FILE *file, File_Location file_loc)
{
    fwrite(file_loc.filename.data, 1, file_loc.filename.size, file);
    fprintf(file, ":%d:%d", file_loc.line, file_loc.column);
}

void PrintTokenValue(FILE *file, const Token *token)
{
    // TODO(henrik): There is a problem, when the token value contains newline
    // characters. Should loop through and transform to escape sequences.
    s64 size = token->value_end - token->value;
    fwrite(token->value, 1, size, file);
}

} // hplang
