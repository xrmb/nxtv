#pragma once
#include "nxlib.h"

#if(1 || defined _DEBUG)
  int __stdcall RR(char** err, const char* fct, int r, const char* m,...);
  #define R(r, m,...) RR(NULL, __FUNCTION__, r, m, __VA_ARGS__)
  #define RE(r, m,...) RR(&m_err, __FUNCTION__, r, m, __VA_ARGS__)
#else
  #define R(r, m) r
#endif

#ifdef _DEBUG
#define NXNew new(_NORMAL_BLOCK, __FILE__, __LINE__)
#include <crtdbg.h>
#define new NXNew
#define NXstrdup(str) _strdup_dbg(str, _NORMAL_BLOCK, __FILE__, __LINE__)
#define NXwcsdup(str) _wcsdup_dbg(str, _NORMAL_BLOCK, __FILE__, __LINE__)
#define NXmalloc(size) _malloc_dbg(size, _NORMAL_BLOCK, __FILE__, __LINE__)
#define NXcalloc(num, size) _calloc_dbg(num, size, _NORMAL_BLOCK, __FILE__, __LINE__)
#define NXrealloc(memblock, size) _realloc_dbg(memblock, size, _NORMAL_BLOCK, __FILE__, __LINE__)
#define NXfree(memblock) _free_dbg(memblock, _NORMAL_BLOCK)
extern wchar_t* NXstrdupw_dbg(const char* str, const char* file, int line);
extern char* NXwcsdupc_dbg(const wchar_t* str, const char* file, int line);
#define NXstrdupw(str) NXstrdupw_dbg(str, __FILE__, __LINE__)
#define NXwcsdupc(str) NXwcsdupc_dbg(str, __FILE__, __LINE__)
#else
#define NXstrdup(str) _strdup(str)
#define NXwcsdup(str) _wcsdup(str)
#define NXmalloc(size) malloc(size)
#define NXcalloc(num, size) calloc(num, size)
#define NXrealloc(memblock, size) realloc(memblock, size)
#define NXfree(memblock) free(memblock)
extern wchar_t* NXstrdupw(const char* str);
extern char* NXwcsdupc(const wchar_t* str);
#endif

extern __time64_t NXtimet32to64(__time32_t t32);
extern __time32_t NXtimet64to32(__time64_t t64);
extern __time64_t NXdostimeto64(__time32_t dostime);
extern __time32_t NXdostimeto32(__time32_t dostime);
extern bool NXfextchk(const wchar_t* fn, const wchar_t* ext);
extern bool NXfextmchk(const char* fn, const char* ext);
extern bool NXfnammchk(const char* fn, const char* nam);
extern void NXfextset(wchar_t* fn, const wchar_t* ext);

#define NXmemdup(data, len) memcpy(NXmalloc(len), data, len)
#define strdupa(a) strcpy((char*)alloca(strlen(a)+1), a)


extern BOOL NXIsNameInExpression(LPCWSTR Expression, LPCWSTR Name, BOOL IgnoreCase);
extern bool NXIsNameInExpression(const char* Expression, const char* Name, bool IgnoreCase);


struct NXLog
{
  char get;
  char head;
  char dupe;
  char cache;
  char mutex;
  char thread;
  char perf;
};

extern NXLog NXLOG;
