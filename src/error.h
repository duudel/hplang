#ifndef H_HPLANG_ERROR_H

#include <cstdio>

namespace hplang
{

struct Error_Context
{
    FILE *file;
    s64 error_count;
};

//void PrintError(File_Location *file_loc, const char *error);
//void PrintError(Token *token, const char *error);

} // hplang

#define H_HPLANG_ERROR_H
#endif
