#ifndef H_HPLANG_ERROR_H

#include <cstdio>
#include "token.h"

namespace hplang
{

enum Compilation_Phase
{
    COMP_None,
    COMP_Lexing,
    COMP_Parsing,
    COMP_Checking,
};

struct Error_Context
{
    FILE *file;
    s64 error_count;
    File_Location first_error_loc;

    Compilation_Phase compilation_phase;
};

void AddError(Error_Context *ctx, File_Location file_loc);

void PrintFileLocation(FILE *file, File_Location file_loc);
void PrintFileLine(FILE *file, Open_File *open_file, File_Location file_loc);
void PrintFileLocArrow(FILE *file, File_Location file_loc);
void PrintTokenValue(FILE *file, const Token *token);

} // hplang

#define H_HPLANG_ERROR_H
#endif
