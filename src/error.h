#ifndef H_HPLANG_ERROR_H

#include <cstdio>
#include "token.h"

namespace hplang
{

struct Error_Context
{
    FILE *file;
    s64 error_count;
    File_Location first_error_loc;
};

void AddError(Error_Context *ctx, File_Location file_loc);

void PrintFileLocation(FILE *file, File_Location file_loc);
void PrintFileLine(FILE *file, File_Location file_loc);
void PrintFileLocArrow(FILE *file, File_Location file_loc);
void PrintTokenValue(FILE *file, const Token *token);

} // hplang

#define H_HPLANG_ERROR_H
#endif
