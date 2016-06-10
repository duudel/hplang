#ifndef H_HPLANG_H

#define HPLANG_VER_MAJOR 0
#define HPLANG_VER_MINOR 1
#define HPLANG_VER_PATCH "0"

#define KBytes(n) (n*1024)
#define MBytes(n) (n*1024*1204)

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#define HP_WIN
#elif defined(linux) || defined(__linux) || defined(__linux__)
#define HP_LINUX
#define HP_UNIX
#elif defined(__unix__)
#define HP_UNIX
#endif

namespace hplang
{

const char* GetVersionString();

} // hplang

#define H_HPLANG_H
#endif
