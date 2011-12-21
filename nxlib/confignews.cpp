#include "nxlib.h"
#include "nxfile.h"



/*static*/ NXLFSConfigNews* NXLFSConfigNews::m_i = NULL;

NXLFSConfigNews::NXLFSConfigNews()
{
  m_port = 119;
  m_host = NULL;
  m_user = NULL;
  m_pass = NULL;
  m_ssl = true;

  m_conn = 1;
  m_timeout = 10;
  m_idle = 60;

  m_cache = NULL;
  m_maxmem = 10*1024*1024;
  m_maxdisk = 10*1024*1024*1024LL;

  m_trustbytes = true;
  m_nzbextfilter = NULL;
  m_nzbnamefilter = NULL;
  m_nzbheadcheck = true;
  m_crc = true;

  m_cfg = NULL;
  m_cfgs = 0;

  if(m_i) fprintf(stderr, "multiple instances of %s\n", "NXLFSConfigNews");
  m_i = this;
}



NXLFSConfigNews::~NXLFSConfigNews()
{
  if(m_i == this) m_i = NULL;

  NXfree(m_host); m_host = NULL;
  NXfree(m_user); m_user = NULL;
  NXfree(m_pass); m_pass = NULL;

  NXfree(m_cache); m_cache = NULL;

  NXfree(m_nzbextfilter); m_nzbextfilter = NULL;
  NXfree(m_nzbnamefilter); m_nzbnamefilter = NULL;

  for(size_t i = 0; i < m_cfgs; i++)
  {
    delete m_cfg[i]; m_cfg[i] = NULL;
  }
  NXfree(m_cfg); m_cfg = NULL;
  m_cfgs = 0;
}



int NXLFSConfigNews::init(const wchar_t* path)
{
  wchar_t p[MAX_PATH];
  wchar_t* s;
  int r;

  if(wcscpy_s(p, MAX_PATH, path)) return R(-11, "buffer error");
  s = wcsrchr(p, L'.');
  if(s && s[1])
  {
    s[1] = L'\0';
    if(wcscat_s(p, MAX_PATH, L"cfg")) return R(-12, "buffer error");
    if(_waccess(p, 0x04) == 0)
    {
      r = init0(p);
      if(r) return r;
    }
  }
  else
  {
    if(_waccess(path, 0x04) == 0)
    {
      r = init0(path);
      if(r) return r;
    }
  }
  
  if(wcscpy_s(p, MAX_PATH, path)) return R(-21, "buffer error");
  s = wcsrchr(p, L'\\');
  if(s && s[1])
  {
    s[1] = L'\0';
    if(wcscat_s(p, MAX_PATH, L"news.cfg")) return R(-22, "buffer error");
    if(_waccess(p, 0x04) == 0)
    {
      r = init0(p);
      if(r) return r;
    }
  }
  else
  {
    wcscpy_s(p, MAX_PATH, L"news.cfg");
    if(_waccess(p, 0x04) == 0)
    {
      r = init0(p);
      if(r) return r;
    }
  }

  if(!m_host) return R(1, "no host");

  return 0;
}



int NXLFSConfigNews::init0(const wchar_t* path)
{
  NXFile* f = new NXFile();
  if(f->load(path)) 
  {
    delete f;
    return R(-1, "nxfile load error");
  }

  m_cfgs++;
  m_cfg = (NXFile**)NXrealloc(m_cfg, sizeof(NXFile*)*m_cfgs);
  m_cfg[m_cfgs-1] = f;

  const char* value;

  value = f->find("host");
  if(value) { NXfree(m_host); m_host = NXstrdup(value); }

  value = f->find("port");
  if(value) { int i = atoi(value); if(m_port < 1 || m_port > 0xFFFF) return R(1, "invalid port value"); m_port = (short)i; }

  value = f->find("user");
  if(value) { NXfree(m_user); m_user = NXstrdup(value); }

  value = f->find("pass");
  if(value) { NXfree(m_pass); m_pass = NXstrdup(value); }

  value = f->find("ssl");
  if(value) { m_ssl = strcmp("1", value) == 0 || _stricmp("true", value) == 0; }


  value = f->find("connections");
  if(value) { m_conn = atol(value); if(m_conn < 1 || m_conn > NXMAXCONN) return R(2, "invalid connections value"); }

  value = f->find("timeout");
  if(value) { m_timeout = atol(value); if(m_timeout < 5 || m_timeout > 60) return R(3, "invalid timeout value"); }

  value = f->find("idle");
  if(value) { m_idle = atol(value); if(m_idle < 60 || m_idle > 3600) return R(4, "invalid idle value"); }


  value = f->find("cache");
  if(value) { NXfree(m_cache); m_cache = NXstrdup(value); }

  value = f->find("maxmem");
  if(value) { m_maxmem = atol(value); if(m_maxmem < 1024*1024 || m_maxmem > 1*1024*1024*1024) return R(5, "invalid maxmem value"); }

  value = f->find("maxdisk");
  if(value) { m_maxdisk = _atoi64(value); if(m_maxdisk < 0 || m_maxdisk > 10*1024*1024*1024LL) return R(6, "invalid maxdisk value"); }


  value = f->find("trustbytes");
  if(value) { m_trustbytes = strcmp("1", value) == 0 || _stricmp("true", value) == 0; }

  value = f->find("nzbextfilter");
  if(value) { NXfree(m_nzbextfilter); m_nzbextfilter = NXstrdup(value); }

  value = f->find("nzbnamefilter");
  if(value) { NXfree(m_nzbnamefilter); m_nzbnamefilter = NXstrdup(value); }

  value = f->find("log");
  if(value) 
  {
    memset(&NXLOG, 0, sizeof(NXLog));
    while(*value)
    {
      switch(*value)
      {
      case 'c': NXLOG.cache++; break;
      case 'd': NXLOG.dupe++; break;
      case 'g': NXLOG.get++; break;
      case 'h': NXLOG.head++; break;
      case 'm': NXLOG.mutex++; break;
      case 'p': NXLOG.perf++; break;
      case 'r': NXLOG.reada++; break;
      case 't': NXLOG.thread++; break;
      }
      value++;
    }
  }

  value = f->find("crc");
  if(value) { m_crc = strcmp("1", value) == 0 || _stricmp("true", value) == 0; }

  return 0;
}



const char* NXLFSConfigNews::find(const char* key) const
{
  for(size_t i = 0; i < m_cfgs; i++)
  {
    const char* v = m_cfg[i]->find(key);
    if(v)
      return v;
  }

  return NULL;
}

