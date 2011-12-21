#pragma once

class NXFile;



class NXLFSConfigNews
{
public:
  NXLFSConfigNews();
  ~NXLFSConfigNews();

  int init(const wchar_t* path);

  const char* host() const { return m_host; }
  short port() const { return m_port; }
  const char* user() const { return m_user; }
  const char* pass() const { return m_pass; }
  long ssl() const { return m_ssl; }
  long conn() const { return m_conn; }
  long timeout() const { return m_timeout; }
  long idle() const { return m_idle; }
  const char* cache() const { return m_cache; }
  long maxmem() const { return m_maxmem; }
  __int64 maxdisk() const { return m_maxdisk; }
  bool trustbytes() const { return m_trustbytes; }
  const char* nzbextfilter() const { return m_nzbextfilter; }
  const char* nzbnamefilter() const { return m_nzbnamefilter; }
  bool nzbheadcheck() const { return m_nzbheadcheck; }
  bool crc() const { return m_crc; }

  void setNzbheadcheck(bool nzbheadcheck) { m_nzbheadcheck = false; }
  void setCache(const char* cache) { NXfree(m_cache); if(cache) m_cache = NXstrdup(cache); else m_cache = NULL; }

  static const NXLFSConfigNews* i() { return m_i; }

  const char* find(const char* key) const;


public:
  static NXLFSConfigNews* m_i;
  int init0(const wchar_t* path);

  char* m_host;
  short m_port;
  char* m_user;
  char* m_pass;
  bool m_ssl;

  long m_conn;
  long m_timeout;
  long m_idle;

  char* m_cache;
  long m_maxmem;
  __int64 m_maxdisk;

  bool m_trustbytes;
  char* m_nzbextfilter;
  char* m_nzbnamefilter;
  bool m_nzbheadcheck;
  bool m_crc;

  NXFile** m_cfg;
  size_t m_cfgs;
};
