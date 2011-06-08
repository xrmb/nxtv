#include "nxlib.h"


NXLFSFolderNXI::NXLFSFolderNXI(const char* path, const wchar_t* name, __time64_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next)
{
  m_path = NXstrdup(path);
}



NXLFSFolderNXI::~NXLFSFolderNXI()
{
  NXfree(m_path); m_path = NULL;
}



/*virtual*/int NXLFSFolderNXI::build()
{
  if(m_build) return m_buildr;
  m_build = true;

  m_buildr = buildr();
  return m_buildr;
}



int NXLFSFolderNXI::buildr()
{
  cleanff();

  int r = NXLFSFileNews::loadnxi(m_path, m_files);
  if(r)
  {
    char msg[MAX_PATH+50];
    sprintf_s(msg, MAX_PATH+50, "loadnxi error %d in %ws", r, m_path);
    m_files = new NXLFSFileText(msg, L"!message.txt", m_time, m_files);
  }

  return buildc();
}
