
#include "hplang.h"

#define PASTE_STR_(x) #x
#define PASTE_STR(x) PASTE_STR_(x)

#define HPLANG_VERSION_STRING\
    "hplang"\
    " "\
    PASTE_STR(HPLANG_VER_MAJOR)\
    "."\
    PASTE_STR(HPLANG_VER_MINOR)\
    "."\
    HPLANG_VER_PATCH

namespace hplang
{

static const char *version_string = HPLANG_VERSION_STRING;

const char* GetVersionString()
{
    return version_string;
}

} // hplang
