#include "nxlib.h"
#include "nxfile.h"
#include "nxhttp.h"



NXLFSFolderRemote::NXLFSFolderRemote(NXLFSFolderRemote::Settings& settings, const char* path, const wchar_t* name, __time64_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next),
  m_settings(settings)
{
  m_path = NXstrdup(path);
}



NXLFSFolderRemote::NXLFSFolderRemote(NXLFSFolderRemote::Settings& settings, const char* path, const char* name, __time32_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next),
  m_settings(settings)
{
  m_path = NXstrdup(path);
}



NXLFSFolderRemote::~NXLFSFolderRemote()
{
  NXfree(m_path); m_path = NULL;
}



int NXLFSFolderRemote::build()
{
  if(m_build) return m_buildr;
  m_build = true;

  m_buildr = buildr();
  return m_buildr;
}



int NXLFSFolderRemote::buildr()
{
  cleanff();

  char uri[MAX_PATH];
  if(strcpy_s(uri, MAX_PATH, m_settings.r)) return R(-1, "buffer error");
  if(strcat_s(uri, MAX_PATH, m_path)) return R(-2, "buffer error");
  //if(sprintf_s(uri, MAX_PATH, "%s%s", m_settings.r, m_path) == -1) return R(-1, "buffer error");

  NXHTTP req;
  if(req.get(uri)) return R(-2, "nxhttp get error");

  NXFile nxf;
  if(nxf.parse(req.body(), req.bodysize())) return R(-3, "nxfile parse error");

  const char* fname = NULL;
  char ftype = '\0';
  const char* fsrc = NULL;
  char fsrctype = '\0';
  __time32_t ftime = -1;
  __int64 fsize = -1;

  const char* k = NULL;
  const char* v = NULL;
  for(;;)
  {
    bool ok = nxf.pair(k, v);
    if(!ok && fname || ok && (strcmp(k, "d") == 0 || strcmp(k, "n") == 0))
    {
      if(fname && ftype == 'd')
      {
        if(ftime == -1) return R(-11, "file time missing");
        if(!fsrc) return R(-12, "file source missing");
        if(fsrctype != 'r') return R(-13, "directory source type invalid");
        m_folders = new NXLFSFolderRemote(m_settings, fsrc, fname, ftime, m_folders);
      }
      
      if(fname && ftype == 'n')
      {
        if(ftime == -1) return R(-14, "file time missing");
        if(!fsrc) return R(-15, "file source missing");
        if(fsize == -1) return R(-16, "no size");
        if(fsrctype == 'r')
        {
          char p[MAX_PATH];
          if(sprintf_s(p, MAX_PATH, "%s/%s", m_settings.r, fsrc) == -1) return R(-17, "buffer error");
          m_files = new NXLFSFileNewsHTTP(p, fname, fsize, ftime, m_files);
        }
        else if(fsrctype == 'i')
        {
          m_files = new NXLFSFileNewsNNTP(fsrc, fname, fsize, ftime, m_files);
        }
        else
        {
          return R(-18, "directory source type invalid");
        }
      }

      if(!ok) break;

      fname = v;
      ftype = k[0];
      fsrc = NULL;
      fsrctype = '\0';
      ftime = -1;
      fsize = -1;

      continue;
    }

    if(!ok) break;

    if(strcmp(k, "s") == 0)
    {
      fsize = _atoi64(v);
      if(fsize <= 0) return R(-21, "file size value error");
      if(fsize == _I64_MAX) return R(-22, "file size value error");
      continue;
    }

    if(strcmp(k, "t") == 0)
    {
      ftime = atol(v);
      if(ftime <= 0) return R(-31, "file time value error");
      if(ftime == LONG_MAX) return R(-32, "file time value error");
      continue;
    }

    if(strcmp(k, "r") == 0)
    {
      fsrc = v;
      fsrctype = k[0];
      continue;
    }

    if(strcmp(k, "i") == 0)
    {
      fsrc = v;
      fsrctype = k[0];
      continue;
    }

    return R(-99, "invalid tag %s", k);
  }
  return 0;
}







NXLFSFolderRemoteRoot::NXLFSFolderRemoteRoot(const wchar_t* nxr, NXLFSFolder* next)
: NXLFSFolderRemote(m_settings, "/", L"root", 0, next)
{
  m_nxr = NXwcsdup(nxr);
  memset(&m_settings, 0, sizeof(Settings));
}



NXLFSFolderRemoteRoot::~NXLFSFolderRemoteRoot()
{
  NXfree(m_nxr); m_nxr = NULL;
}



int NXLFSFolderRemoteRoot::build()
{
  if(m_build) return m_buildr;
  m_build = true;

  m_buildr = buildr();
  return m_buildr;
}



int NXLFSFolderRemoteRoot::buildr()
{
  NXFile nxr;
  if(nxr.load(m_nxr)) return R(-1, "nxfile load error");

  const char* v = NULL;
  v = nxr.find("r");
  if(!v) return R(-2, "no r");
  if(strcpy_s(m_settings.r, MAX_PATH, v)) return R(-3, "buffer error");

  v = nxr.find("c");
  if(v && strcpy_s(m_settings.c, MAX_PATH, v)) return R(-4, "buffer error");

  return NXLFSFolderRemote::buildr();
}


