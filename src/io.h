#ifndef H_HPLANG_IO_H

#include "types.h"

namespace hplang
{

    // Type punning of c-standard FILE. The dummies who specified what FILE is
    // did not specify it would be struct, so now it is a typedef of an unknown
    // type in most implementations. This makes it impossible to forward
    // declare FILE in the headers, forcing to include stdio. Unless... we use
    // type punning... which is not the most elegant solution.
    struct IoFile;

    s64 PrintString(IoFile *file, String str);

    inline s64 PrintName(IoFile *file, Name name)
    { return PrintString(file, name.str); }

} //hplang

#define H_HPLANG_IO_H
#endif
