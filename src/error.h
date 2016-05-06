#ifndef H_HPLANG_ERROR_H

#include "token.h"
#include "io.h"

namespace hplang
{

struct Error_Context
{
    IoFile *file;
    s64 error_count;
    File_Location first_error_loc;
};

void AddError(Error_Context *ctx, File_Location file_loc);

void PrintFileLocation(IoFile *file, File_Location file_loc);
void PrintFileLine(IoFile *file, File_Location file_loc);
void PrintFileLocArrow(IoFile *file, File_Location file_loc);
void PrintTokenValue(IoFile *file, const Token *token);

} // hplang

#define H_HPLANG_ERROR_H
#endif
