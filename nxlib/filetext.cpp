#include "nxlib.h"
#include "nxlfs.h"



NXLFSFileText::NXLFSFileText(const char* text, const wchar_t* name, __time64_t time, NXLFSFile* next)
: NXLFSFile(name, time, next)
{
  m_text = NXstrdup(text);
  m_size = strlen(m_text);

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileText::NXLFSFileText(const char* text, const char* name, __time32_t time, NXLFSFile* next)
: NXLFSFile(name, time, next)
{
  m_text = NXstrdup(text);
  m_size = strlen(m_text);

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileText::~NXLFSFileText()
{
  NXfree(m_text); m_text = NULL;

  if(NXLFS::i()) NXLFS::i()->rmSize(m_size);
}



/*virtual*/int NXLFSFileText::open()
{
  return 0;
}



/*virtual*/int NXLFSFileText::read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int /*readahead*/, NXLFSFile* /*rahint*/) 
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + *rlen > m_size)
  {
    *rlen = (size_t)(m_size - offset);
  }
  if(*rlen == 0) return 0;

  if(memcpy_s(buffer, blen, m_text+offset, *rlen)) return R(-2, "buffer size error");

  return 0;
}



/*virtual*/int NXLFSFileText::close() 
{
  return 0;
}



