#include "nxlib.h"



NXLFSFile::NXLFSFile(const wchar_t* name, __time64_t time, NXLFSFile* next)
: m_name(NXwcsdup(name)),
  m_time(time),
  m_next(next),
  m_size(0),
  m_head(NULL)
{
}



NXLFSFile::NXLFSFile(const char* name, __time32_t time, NXLFSFile* next)
: m_name(NXstrdupw(name)),
  m_time(NXtimet32to64(time)),
  m_next(next),
  m_size(0),
  m_head(NULL)
{
}



NXLFSFile::~NXLFSFile()
{
  //--- really long lists can cause stack overflow ---
  //delete m_next; m_next = NULL;
  while(m_next)
  {
    NXLFSFile* d = m_next;
    m_next = m_next->m_next;
    d->m_next = NULL;
    delete d;
  }

  NXfree(m_name); m_name = NULL;
  NXfree(m_head); m_head = NULL;
}



int NXLFSFile::rename(const wchar_t* name)
{
  size_t len = wcslen(name)+1;
  m_name = (wchar_t*)NXrealloc(m_name, sizeof(wchar_t)*len);
  if(!m_name) return R(-1, "realloc error"); // it will go down from here on!
  if(wcscpy_s(m_name, len, name)) return R(-2, "buffer error");
  return 0;
}
