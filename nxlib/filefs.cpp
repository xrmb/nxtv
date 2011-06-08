#include "nxlib.h"
#include "nxlfs.h"



NXLFSFileFS::NXLFSFileFS(const wchar_t* path, __int64 size, const wchar_t* name, __time64_t time, NXLFSFile* next)
: NXLFSFile(name, time, next)
{
  m_path = NXwcsdupc(path);
  m_size = size;
  m_fh = NULL;

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileFS::NXLFSFileFS(const char* path, __int64 size, const wchar_t* name, __time64_t time, NXLFSFile* next)
: NXLFSFile(name, time, next)
{
  m_path = NXstrdup(path);
  m_size = size;
  m_fh = NULL;

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileFS::~NXLFSFileFS()
{
  close();
  NXfree(m_path); m_path = NULL;

  if(NXLFS::i()) NXLFS::i()->rmSize(m_size);
}



/*virtual*/int NXLFSFileFS::open()
{
  close();
  if(fopen_s(&m_fh, m_path, "rb")) return R(-1, "file open error");
  if(m_fh == NULL) return R(-2, "invalid file handle");
  return 0;
}



/*virtual*/int NXLFSFileFS::read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int /*readahead*/, NXLFSFile* /*rahint*/) 
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + *rlen > m_size)
  {
    *rlen = (size_t)(m_size - offset);
  }
  if(*rlen == 0) return 0;

  if(!m_fh && open()) return R(-2, "open error");

  if(_fseeki64(m_fh, offset, SEEK_SET)) return R(-3, "seek error");
  if(offset != _ftelli64(m_fh)) return R(-4, "seek error");

  *rlen = fread_s(buffer, blen, 1, *rlen, m_fh); // ERR

  if(ferror(m_fh)) return R(-5, "read error");
  return 0;
}



/*virtual*/int NXLFSFileFS::close() 
{
  if(m_fh)
  {
    int r = fclose(m_fh);
    m_fh = NULL;
    if(r) return R(-1, "file close error");
  }
  return 0;
}



