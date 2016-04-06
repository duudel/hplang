#ifndef H_HPLANG_ERROR_H

#include <cstdio>

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

//void PrintError(File_Location *file_loc, const char *error);
//void PrintError(Token *token, const char *error);

} // hplang

#define H_HPLANG_ERROR_H
#endif
