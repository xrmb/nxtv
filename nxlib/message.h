#pragma once

#include "nxthread.h"



class NXMutex;



const char NXPARTVERSION = '2';
#pragma pack(push)
#pragma pack(1)
struct NXPart
{                         
  char version;           
  __int64 size;           // filesize
  __int64 begin;
  __int32 blocksize;      
  unsigned __int32 crc32;
  unsigned __int32 len;   // data buffer length
  char msgid[NXMAXMSGID];
  char data;             
};
#pragma pack(pop)



class NXLFSMessageCache
{
public:
  NXLFSMessageCache(const char* cache, __int64 maxdisk, size_t maxmem);
  ~NXLFSMessageCache();

  static NXLFSMessageCache* i() { return m_i; }
  int aget(const char* msgid);
  int get(const char* msgid, void* dest, size_t offset, size_t count);
  int get(const char* msgid, NXPart*& dest);
  int find(const char* msgid, NXPart* dest, size_t count = -1);

  int store(const NXPart* data, int oid);

  int oid();
  int init();
  int cfcleanup();

private:
  class D
  {
  public:
    D(const char* msgid, int oid, D* next) : i(oid), n(next) { strcpy_s(d, NXMAXMSGID, msgid); }
    ~D();

    char d[NXMAXMSGID];
    int i;
    D* n;
  };

  class CF
  {
  public:
    CF(int slot, const char* msgid, size_t size, time_t time, CF* next) : s(slot), z(size), t(time), c(false), n(next) { strcpy_s(d, NXMAXMSGID, msgid); }
    ~CF();

    char d[NXMAXMSGID]; // msgid
    int s;    // slot
    time_t t; // time
    size_t z; // size
    bool c;   // changed
    CF* n;    // next
  };

  //const NXPart* get(const char* msgid);
  const NXPart* find(const char* msgid);
  int fcached(const char* msgid) const;
  const NXPart* fetch(const char* msgid);
  const NXPart* insert(NXPart* data, int oid);
  const NXPart* insert(const NXPart* data, int oid);
  bool dupe(const char* msgid, int oid);
  int slot(const char* msgid) const;
  static int cfsort(const void* a, const void* b);
  void cfuse(const char* msgid, size_t size);
  void trim();

  D* m_dupe;
  CF* m_files;

  NXPart** m_list;
  size_t m_listlen;

  static NXLFSMessageCache* m_i;
  char* m_cache;
  __int64 m_maxdisk;
  size_t m_maxmem;
  NXMutex* m_mutex;
  NXThread<NXLFSMessageCache>* m_cleanup;
  int m_oid;   // order id, increases with each get/aget/fetch, used 
};
