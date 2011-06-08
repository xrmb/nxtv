#include "nxlib.h"



NXLFSFolderFS::NXLFSFolderFS(const wchar_t* path, const wchar_t* name, __time64_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next)
{
  m_path = NXwcsdup(path);
}



NXLFSFolderFS::~NXLFSFolderFS()
{
  NXfree(m_path); m_path = NULL;
}



int NXLFSFolderFS::build()
{
  if(m_build) return m_buildr;
  m_build = true;

  m_buildr = buildr();
  return m_buildr;
}



int NXLFSFolderFS::buildr()
{
  cleanff();

  intptr_t r;
  _wfinddata64_t fi;
  wchar_t p0[MAX_PATH];

  if(wcscpy_s(p0, MAX_PATH, m_path)) return R(-1, "buffer error");
  if(wcscat_s(p0, MAX_PATH, L"\\*")) return R(-2, "buffer error");

  r = _wfindfirst64(p0, &fi);
  if(r != -1L)
  {
    do
    {
      wcscpy_s(p0, MAX_PATH, m_path); // no err check, just did it a view lines ago
      wcscat_s(p0, MAX_PATH, L"\\");  // -::-
      if(wcscat_s(p0, MAX_PATH, fi.name)) return R(-3, "buffer error");

      if(fi.attrib & _A_SUBDIR)
      {
        if(_wcsicmp(fi.name, L".") != 0 && _wcsicmp(fi.name, L"..") != 0)
        {
          m_folders = new NXLFSFolderFS(p0, fi.name, fi.time_create, m_folders);
        }
      }
      else
      {
        size_t l = wcslen(fi.name);
        if(_wcsicmp(fi.name+l-4, L".nzb") == 0)
        {
          m_folders = new NXLFSFolderNZB(p0, fi.name, fi.time_create, m_folders);
        }
        else if(_wcsicmp(fi.name+l-4, L".nxr") == 0)
        {
          NXLFSFolder* f = new NXLFSFolderRemoteRoot(p0);
          f->build();
          adopt(f);
        }
        else if(_wcsicmp(fi.name+l-4, L".nxi") == 0)
        {
          char pa0[MAX_PATH];
          if(wcstombs_s(NULL, pa0, p0, MAX_PATH)) return R(-4, "convert error");

          char nxh[MAX_PATH];
          strcpy_s(nxh, MAX_PATH, pa0);
          nxh[strlen(pa0)-1] = 'h';

          if(_access(nxh, 0x04) == 0)
          {
            int r = NXLFSFileNews::loadnxh(nxh, pa0, m_files);
            if(r)
            {
              char msg[MAX_PATH+50];
              sprintf_s(msg, MAX_PATH+50, "loadnxh error %d in %s", r, nxh);
              m_files = new NXLFSFileText(msg, nxh+wcslen(m_path), NXtimet64to32(fi.time_create), m_files);
            }
          }
          else
          {
            int r = NXLFSFileNews::loadnxi(pa0, m_files);
            if(r)
            {
              char msg[MAX_PATH+50];
              sprintf_s(msg, MAX_PATH+50, "loadnxi error %d in %s", r, p0);
              m_files = new NXLFSFileText(msg, fi.name, fi.time_create, m_files);
            }
          }
        }
        else if(_wcsicmp(fi.name+l-4, L".nxh") == 0)
        {
          //--- ignore, will be picked up by nxi ---
        }
        else
        {
          m_files = new NXLFSFileFS(p0, fi.size, fi.name, fi.time_create, m_files);
        }
      }
    } while(_wfindnext64(r, &fi) == 0);
    
    _findclose(r);
  }

  return buildc();
}


