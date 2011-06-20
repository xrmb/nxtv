#include "nxlib.h"

#include "nxmutex.h"


/*static*/ NXLFSMessageCache* NXLFSMessageCache::m_i = NULL;


unsigned long __stdcall NXLFSMessageCacheCleanup(void* nxthread)
{
  NXThread<NXLFSMessageCache>* t = reinterpret_cast<NXThread<NXLFSMessageCache>*>(nxthread);
  if(!t) { return 1; }

  NXLFSMessageCache* cache = t->data();
  if(!cache) { return t->exit(1); }

  for(;;)
  {
    t->suspend(60000);

    if(t->terminated())
      break;

    cache->cfcleanup();
  }

  return t->exit(0);
}



NXLFSMessageCache::NXLFSMessageCache(const char* cache, __int64 maxdisk, size_t maxmem)
{
  m_cache = cache ? NXstrdup(cache) : NULL;
  m_maxdisk = maxdisk;
  m_maxmem = maxmem;
  m_dupe = NULL;
  m_mutex = new NXMutex("NXLFSMessageCache");
  m_oid = -1;
  m_files = NULL;
  m_listlen = maxmem / 384000;
  m_list = (NXPart**)NXcalloc(m_listlen, sizeof(NXPart*));

  m_cleanup = new NXThread<NXLFSMessageCache>(NXLFSMessageCacheCleanup, this);

  if(m_i) fprintf(stderr, "multiple instances of %s\n", "NXLFSMessageCache");
  m_i = this;
}



NXLFSMessageCache::~NXLFSMessageCache()
{
  if(m_i == this) m_i = NULL;

  delete m_cleanup; m_cleanup = NULL;
  cfcleanup();

  for(size_t i = 0; i < m_listlen; i++)
  {
    NXfree(m_list[i]); m_list[i] = NULL;
  }
  NXfree(m_list); m_list = NULL;

  NXfree(m_cache); m_cache = NULL;

  delete m_dupe; m_dupe = NULL;
  delete m_files; m_files = NULL;

  delete m_mutex; m_mutex = NULL;
}



/*static*/int NXLFSMessageCache::cfsort(const void* a, const void* b)
{
  NXLFSMessageCache::CF** ca = (NXLFSMessageCache::CF**)a;
  NXLFSMessageCache::CF** cb = (NXLFSMessageCache::CF**)b;
  return (int)((*cb)->t - (*ca)->t);
}



int NXLFSMessageCache::init()
{
  if(!m_cache) return 0;
  
  if(_access(m_cache, 0x00) && _mkdir(m_cache)) return R(-1, "cant create cache folder");
  if(_access(m_cache, 0x06)) return R(-2, "cant access cache folder");
  
  char p[MAX_PATH];
  char* f[] = { "rar", "nzb", "msg", "rem" };
  for(int j = 0; j < 4; j++)
  {
    if(sprintf_s(p, MAX_PATH, "%s\\%s", m_cache, f[j]) == 1) return R(-11, "buffer error");
    if(_access(p, 0x00) && _mkdir(p)) return R(-12, "cant create %s cache folder", f[j]);
    if(_access(p, 0x06)) return R(-13, "cant access %s cache folder", f[j]);
    for(int i = 0; i < 16; i++)
    {
      if(sprintf_s(p, MAX_PATH, "%s\\%s\\%X", m_cache, f[j], i) == 1) return R(-14, "buffer error");
      if(_access(p, 0x00) && _mkdir(p)) return R(-15, "cant create %s cache folder", f[j]);
      if(_access(p, 0x06)) return R(-16, "cant access %s cache folder", f[j]);
    }
  }

  clock_t start = clock();
  size_t cnt = 0;
  for(int i = 0; i < 16; i++)
  {
    if(sprintf_s(p, MAX_PATH, "%s\\msg\\%X\\*", m_cache, i) == 1) return R(-31, "buffer error");
    _finddata_t fd;
    intptr_t dh = _findfirst(p, &fd);
    if(dh == -1) continue;
    for(;_findnext(dh, &fd) == 0;)
    {
      if(fd.attrib & _A_SUBDIR) continue;
      m_files = new CF(i, fd.name, fd.size, fd.time_write, m_files);
      cnt++;
    }
    
    _findclose(dh);
  }
  clock_t end = clock();
  if(NXLOG.cache)
    printf("cache:\tscan took %.2fsec\n", (double)(end-start)/CLOCKS_PER_SEC);

  if(cnt > 1)
  {
    CF** cfs = (CF**)NXalloca(sizeof(CF*)*cnt);
    CF* cf = m_files;
    for(size_t i = 0; i < cnt; i++, cf = cf->n)
      cfs[i] = cf;

    qsort(cfs, cnt, sizeof(CF*), cfsort);

    for(size_t i = 0; i < cnt-1; i++)
      cfs[i]->n = cfs[i+1];
    cfs[cnt-1]->n = NULL;
    m_files = cfs[0];
  }

  return 0;
}



int NXLFSMessageCache::slot(const char* msgid) const
{
  int c = 0;
  while(msgid && *msgid)
  {
    c += *msgid;
    msgid++;
  }
  return c % 16;
}



int NXLFSMessageCache::store(const NXPart* data, int oid)
{
  insert(data, oid);

  if(!m_cache || !m_maxdisk) return 0;

  NXMutexLocker ml(m_mutex, "store", data->msgid);

  char p[MAX_PATH];
  if(sprintf_s(p, "%s\\%s\\%X\\%s", m_cache, "msg", slot(data->msgid), data->msgid) == -1) return R(-1, "buffer error");

  int fd = -1;
  if(_sopen_s(&fd, p, _O_BINARY | _O_WRONLY | _O_CREAT, _SH_DENYWR, _S_IWRITE)) return R(-11, "open error");
  if(fd == -1) return R(-12, "no file descriptor");

  size_t len = sizeof(NXPart) + data->len - 1;
  if(len != _write(fd, data, len))
  {
    _close(fd);
    return R(-13, "write error");
  }
  _close(fd); // ERR

  return 0;
}



const NXPart* NXLFSMessageCache::fetch(const char* msgid)
{
  if(!m_cache || !m_maxdisk) return NULL;

  int r;
  char p[MAX_PATH];
  if(sprintf_s(p, "%s\\%s\\%X\\%s", m_cache, "msg", slot(msgid), msgid) == -1) { r = R(-1, "buffer error"); return NULL; }

  int fd = -1;
  if(_sopen_s(&fd, p, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IWRITE)) return NULL;
  if(fd == -1) return NULL;

  auto_close acfd(fd);

  struct stat st;
  if(fstat(fd, &st)) { r = R(-2, "stat error"); return NULL; }
  size_t size = st.st_size;

  if(size <= sizeof(NXPart)) { r = R(-3, "file too small"); return NULL; }

  auto_free<char> data0(size);
  if(size != _read(fd, data0, size)) { r = R(-4, "read error"); return NULL; }

  acfd.close();

  NXPart* data = (NXPart*)((char*)data0);// todo: review
  if(data->version != NXPARTVERSION)
  {
    _unlink(p);
    r = R(-5, "wrong part version"); 
    return NULL; 
  }

  if(strcmp(data->msgid, msgid))
  {
    _unlink(p);
    r = R(-6, "msgid mismatch"); 
    return NULL; 
  }

  if(data->len != size - (sizeof(NXPart)-1))
  {
    _unlink(p);
    r = R(-7, "length mismatch"); 
    return NULL; 
  }

  //data->len = size - (sizeof(NXPart) - 1);

  cfuse(msgid, size);

  data0.release();
  return insert(data, oid());
}



int NXLFSMessageCache::fcached(const char* msgid) const
{
  if(!m_cache || !m_maxdisk) return 1;

  char p[MAX_PATH];
  if(sprintf_s(p, "%s\\%s\\%X\\%s", m_cache, "msg", slot(msgid), msgid) == -1) return R(-1, "buffer error");

  return _access_s(p, 0x04);
}


/*
  returns:
  >0x10000  ... connection now working on it
  >0x20000  ... connection already working on it
  =0        ... all connections busy
  <0        ... somewhere in cache
*/
int NXLFSMessageCache::aget(const char* msgid)
{
  if(find(msgid)) return -1;

  if(fcached(msgid) == 0) return -2;

  NXNNTPPool* pool = NXNNTPPool::i();
  if(!pool) return R(-3, "no pool");
  return pool->aget(msgid);
}



const NXPart* NXLFSMessageCache::find(const char* msgid)
{
  NXMutexLocker ml(m_mutex, "find", msgid);

  for(size_t i = 0; i < m_listlen; i++)
  {
    const NXPart* p = m_list[(m_listlen + m_oid - i) % m_listlen];
    if(p && strcmp(msgid, p->msgid) == 0)
    {
      cfuse(msgid, p->len); // we lose sizeof(NXPart)
      return p;
    }
  }

  return NULL;
}



bool NXLFSMessageCache::dupe(const char* msgid, int oid)
{
  if(!NXLOG.dupe)
    return false;

  D* f = m_dupe;
  while(f)
  {
    if(strcmp(msgid, f->d) == 0)
    {
      printf("dupe:\t%-10s %d\t%d\t%d\t%s\n", "dupecheck", f->i, oid, m_oid, msgid);
      return true;
    }
    f = f->n;
  }

  m_dupe = new D(msgid, oid, m_dupe);

  return false;
}



const NXPart* NXLFSMessageCache::insert(NXPart* data, int oid)
{
  NXMutexLocker ml(m_mutex, "insert1", data->msgid);

  if(NXLOG.cache > 1)
    printf("cache:\t%-10s %s\n", "insert1", data->msgid);
  dupe(data->msgid, oid);

  trim();

  oid %= m_listlen;

  NXfree(m_list[oid]);
  m_list[oid] = data;

  return m_list[oid];
}



const NXPart* NXLFSMessageCache::insert(const NXPart* data, int oid)
{
  NXMutexLocker ml(m_mutex, "insert2", data->msgid);

  if(NXLOG.cache > 1)
    printf("cache:\t%-10s %s\n", "insert2", data->msgid);
  dupe(data->msgid, oid);

  trim();
  
  size_t size = data->len + sizeof(NXPart) - 1;
  oid %= m_listlen;

  //--- reuse memory ---
  if(m_list[oid] == NULL || _msize(m_list[oid]) < size)
  {
    m_list[oid] = (NXPart*)NXrealloc(m_list[oid], size);
  }
  memcpy_s(m_list[oid], size, data, size); // ERR


  return m_list[oid];
}



void NXLFSMessageCache::trim()
{
  size_t m = 0;
  size_t r = 0;
  int c = 0;
  long conn = 10;
  
  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  if(cfg) conn = cfg->conn();

  for(size_t i = 0; i < m_listlen; i++)
  {
    int j = (m_listlen + m_oid - i) % m_listlen;
    if(!m_list[j]) continue;

    m += m_list[j]->len;    // data
    r += _msize(m_list[j]); // real memory, not everything is used
    c++;
    
    //--- note: still debating if r > or m > is better ---
    //          r > is more accurate to maxmem, but heap fragmenation 
    if(c > conn && r > m_maxmem)
    {      
      if(NXLOG.cache > 2)
        printf("cache:\t%-10s %d\t%d\t%s\n", "delete", j, m_oid, m_list[j]->msgid);
      NXfree(m_list[j]); m_list[j] = NULL;
    }
  }
  if(NXLOG.cache > 1)
    printf("cache:\t%-10s %d items, %d bytes, %d mem, %d oid\n", "stats", c, m, r, m_oid);
}



int NXLFSMessageCache::oid() 
{ 
  NXMutexLocker ml(m_mutex, "oid", "n/a");

  m_oid++; 

  return m_oid; 
}



int NXLFSMessageCache::cfcleanup()
{
  NXMutexLocker ml(m_mutex, "cfcleanup", "n/a");

  char p[MAX_PATH];
  int r = 0;

  clock_t start = clock();

  //--- touch recently used cache files ---
  for(CF* f = m_files; f; f = f->n)
  {
    if(!f->c) break;
    if(sprintf_s(p, "%s\\%s\\%X\\%s", m_cache, "msg", f->s, f->d) == -1) { r = R(-11, "buffer error"); continue; }

    _utimbuf tb;
    tb.actime = tb.modtime = f->t;
    if(_utime(p, &tb)) r = R(-21, "utime error");
  }

  //--- remove cache files over limit ---
  __int64 s = m_maxdisk;
  for(CF* f = m_files; f; f = f->n)
  {
    s -= f->z;
    if(s < 0)
    {
      CF* c = f->n;
      f->n = NULL;
      ml.unlock(); // todo: what if someone fetches an article delete in a few nanos? stop worrying

      for(CF* d = c; d; d = d->n)
      {
        if(sprintf_s(p, "%s\\%s\\%X\\%s", m_cache, "msg", d->s, d->d) == -1) { r = R(-21, "buffer error"); continue; }
        if(_unlink(p)) { r = R(-21, "unlink error"); continue; }
        if(NXLOG.cache > 1)
          printf("cache:\tcleanup %s\n", d->d);
      }
      delete c; 
    }
  }

  clock_t end = clock();
  if(NXLOG.cache)
    printf("cache:\tcleanup took %.2fsec\n", (double)(end-start)/CLOCKS_PER_SEC);

  return r;
}



void NXLFSMessageCache::cfuse(const char* msgid, size_t size)
{
  if(!m_cache || !m_maxdisk) return;

  NXMutexLocker ml(m_mutex, "cfuse", "n/a");

  CF* cfl = NULL;
  for(CF* cf = m_files; cf; cf = cf->n)
  {
    if(strcmp(cf->d, msgid) == 0)
    {
      if(cfl) 
      {
        cfl->n = cf->n;
        cf->n = m_files;
        m_files = cf;
      }
      m_files->c = true;
      m_files->t = time(NULL);
      return;
    }
    cfl = cf;
  }

  m_files = new CF(slot(msgid), msgid, size, time(NULL), m_files);
}



int NXLFSMessageCache::get(const char* msgid, void* dest, size_t offset, size_t count)
{
  NXMutexLocker ml(m_mutex, "get", msgid);
  const NXPart* p = NULL;
  for(;;)
  {
    p = find(msgid);
    if(p) break;

    p = fetch(msgid);
    if(p) break;
    
    ml.unlock();
    NXNNTPPool* pool = NXNNTPPool::i();
    if(!pool) return R(-1, "no pool");
    if(pool->get(msgid)) return R(-2, "get error");
    
    ml.lock();
    p = find(msgid);
    if(!p) return R(-3, "message data vanished");
    break;
  }

  if(offset > p->len) return R(-4, "offset out of range (%d/%d)", offset, p->len);
  if(offset + count > p->len) return R(-5, "end out of range (%d/%d)", offset, count > p->len);

  memcpy(dest, &p->data+offset, count);

  return 0;
}



int NXLFSMessageCache::get(const char* msgid, NXPart*& dest)
{
  NXMutexLocker ml(m_mutex, "get", msgid);
  const NXPart* p = NULL;
  for(;;)
  {
    p = find(msgid);
    if(p) break;

    p = fetch(msgid);
    if(p) break;
    
    ml.unlock();
    NXNNTPPool* pool = NXNNTPPool::i();
    if(!pool) return R(-1, "no pool");
    if(pool->get(msgid)) return R(-2, "get error");
    
    ml.lock();
    p = find(msgid);
    if(!p) return R(-3, "message data vanished");
    break;
  }

  dest = (NXPart*)NXmemdup(p, sizeof(NXPart) + p->len);

  return 0;
}



int NXLFSMessageCache::find(const char* msgid, NXPart* dest, size_t count)
{
  NXMutexLocker ml(m_mutex, "find+copy", msgid);
  const NXPart* p = find(msgid);
  if(!p) return -1;

  if(count == -1) count = sizeof(NXPart) + p->len;
  if(count > sizeof(NXPart) + p->len) count = sizeof(NXPart) + p->len;
  if(count < sizeof(NXPart)) return R(-2, "count out of range");

  memcpy(dest, p, count);

  return count;
}



NXLFSMessageCache::D::~D() 
{ 
  //--- really long lists can cause stack overflow ---
  while(n)
  {
    D* d = n;
    n = n->n;
    d->n = NULL;
    delete d;
  }
}



NXLFSMessageCache::CF::~CF() 
{ 
  //--- really long lists can cause stack overflow ---
  while(n)
  {
    CF* d = n;
    n = n->n;
    d->n = NULL;
    delete d;
  }
}
