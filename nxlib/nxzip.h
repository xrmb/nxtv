#pragma once

#include "nxlib.h"

//#define NXDYNZ

#ifdef NXDYNZ
#define BZ_NO_STDIO
#include <bzlib.h>

#define ZLIB_DLL
#include <zlib.h>
#undef inflateInit

#pragma comment(lib, "zdll.lib")
#pragma comment(lib, "libbz2.lib")

class NXbzip2
{
public:
  int decompressInit(bz_stream* strm, int verbosity, int small) { return BZ2_bzDecompressInit(strm, verbosity, small); }
  int decompress(bz_stream* strm) { return BZ2_bzDecompress(strm); }
  int decompressEnd(bz_stream* strm) { return BZ2_bzDecompressEnd(strm); }

  int load() { return 0; }
};


class NXgzip
{
public:
  int inflateInit(z_streamp strm, int windowBits, const char* version, int stream_size) { return inflateInit2_(strm, windowBits, version, stream_size); }
  int inflate(z_streamp strm, int flush) { return ::inflate(strm, flush); }
  int inflateEnd(z_streamp strm) { return ::inflateEnd(strm); }

  int load() { return 0; }
};
#else
  #define BZ_IMPORT
  #define BZ_NO_STDIO
  #include <bzlib.h>

  #define ZLIB_DLL
  #include <zlib.h>
  #undef inflateInit

#ifdef _WIN32
#include <io.h>
#endif

#include <windows.h>



class NXbzip2
{
public:
  NXbzip2() 
  : m_h(NULL),
    decompressInit(NULL),
    decompress(NULL),
    decompressEnd(NULL)
  {}

  ~NXbzip2()
  {
    if(m_h)
      unload();
  }

  int (__stdcall *decompressInit)(bz_stream*, int, int);
  int (__stdcall *decompress)(bz_stream*);
  int (__stdcall *decompressEnd)(bz_stream*);

  int load()
  {
    if(m_h) return 0;
    m_h = LoadLibraryA("libbz2.dll");
    if(m_h == NULL)
      return R(-1, "error loading libbz2.dll");

    decompressInit = (int (__stdcall*)(bz_stream*, int, int))GetProcAddress(m_h, "BZ2_bzDecompressInit");
    decompress = (int (__stdcall*)(bz_stream*))GetProcAddress(m_h, "BZ2_bzDecompress");
    decompressEnd = (int (__stdcall*)(bz_stream*))GetProcAddress(m_h, "BZ2_bzDecompressEnd");

    if( !decompressInit ||
        !decompress ||
        !decompressEnd) 
    {
      decompressInit = NULL;
      decompress = NULL;
      decompressEnd = NULL;

      return R(-2, "GetProcAddress failed");
    }

    return 0;
  }


private:
  int unload()
  {
    if(!m_h) return R(-1, "not loaded");
    FreeLibrary(m_h);
    m_h = NULL;
    decompressInit = NULL;
    decompress = NULL;
    decompressEnd = NULL;
    return 0;
  }

  HMODULE m_h;
};


class NXgzip
{
public:
  NXgzip() 
  : m_h(NULL),
    inflateInit(NULL),
    inflate(NULL),
    inflateEnd(NULL)
  {}

  ~NXgzip()
  {
    if(m_h)
      unload();
  }
  
  int (*inflateInit)(z_streamp strm, int windowBits, const char* version, int stream_size);
  int (*inflate)(z_streamp strm, int flush);
  int (*inflateEnd)(z_streamp strm);

  int load()
  {
    if(m_h) return 0;
    m_h = LoadLibraryA("zlib1.dll");
    if(m_h == NULL)
      return R(-1, "error loading zlib1.dll");

    inflateInit = (int (*)(z_streamp, int, const char*, int))GetProcAddress(m_h, "inflateInit2_");
    inflate = (int (*)(z_streamp, int))GetProcAddress(m_h, "inflate");
    inflateEnd = (int (*)(z_streamp))GetProcAddress(m_h, "inflateEnd");

    if( !inflateInit ||
        !inflate ||
        !inflateEnd) 
    {
      inflateInit = NULL;
      inflate = NULL;
      inflateEnd = NULL;

      return R(-2, "GetProcAddress failed");
    }

    return 0;
  }


private:
  int unload()
  {
    if(!m_h) return R(-1, "not loaded");
    FreeLibrary(m_h);
    m_h = NULL;
    inflateInit = NULL;
    inflate = NULL;
    inflateEnd = NULL;
    return 0;
  }

  HMODULE m_h;
};
#endif


NXgzip NXGZIP;
NXbzip2 NXBZIP2;
