#include "nxlib.h"



NXLFSFileRAR::NXLFSFileRAR(Piece* pieces, const char* name, __time32_t time, NXLFSFile* next)
: NXLFSFile(name, time, next),
  m_pieces(pieces),
  m_lrf(NULL)
{
  m_size = pieces->size();

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileRAR::~NXLFSFileRAR()
{
  close();
  delete m_pieces; m_pieces = NULL;

  if(NXLFS::i()) NXLFS::i()->rmSize(m_size);
}



/*virtual*/int NXLFSFileRAR::open() 
{ 
  return 0; 
}



/*virtual*/int NXLFSFileRAR::read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint)
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + *rlen > m_size)
  {
    *rlen = (size_t)(m_size - offset);
  }
  if(*rlen == 0) return 0;

  __int64 pos = offset;
  size_t boffset = 0;
  Piece* p;
  for(p = m_pieces; p; p = p->n)
  {
    if(pos < p->len)
    {
      boffset = (size_t)pos;
      break;
    }
    pos -= p->len;
  }
  // todo: if we do everything right that should not happen
  if(p == NULL) return R(-2, "piece limit reached");

  size_t tg = *rlen;
  char* cbuffer = (char*)buffer;
  while(tg)
  {
    // todo: if we do everything right that should not happen
    if(p == NULL) return R(-3, "piece limit reached");

    if(m_lrf && p->f != m_lrf) 
    {
      m_lrf->close();
      m_lrf = p->f;
    }

    size_t w = min(p->len-boffset, tg);
    size_t r = w;
    if(p->f->read(cbuffer, w, &r, p->pos + boffset, readahead, p->n ? p->n->f : NULL)) return R(-4, "read error");
    if(r != w) return R(-5, "read error");

    tg -= w;
    cbuffer += w;
    p = p->n;
    boffset = 0;
  }

  return 0;
}



/*virtual*/int NXLFSFileRAR::close()
{
  int r = 0;
  if(m_lrf) r = m_lrf->close();
  m_lrf = NULL;
  return r;
}



/*static*/int NXLFSFileRAR::analyze(NXLFSFileRAR::Fni& fni, const wchar_t* file)
{
  memset(&fni, 0, sizeof(Fni));

  size_t len = wcslen(file);
  if(len < 5) return -1;
  if(file[len-1] == L'!')
  {
    fni.kaputt = true;
    len--;
  }
  if(file[len-4] == L'.' && towlower(file[len-3]) == L'r' && iswdigit(file[len-2]) && iswdigit(file[len-1]))
  {
    fni.israr = true;
    fni.bnl = len - 4;
    fni.part = _wtoi(file+len-2)+1;
    fni.ns = 0;
    return 0;
  }
  if(_wcsnicmp(file+len-4, L".rar", 4) != 0) return -2;

  size_t n = len-5; // -5 ... length of .rar +1 char before that
  int c = 0;
  while(n > 5 && iswdigit(file[n])) // >5 ... length of .part
  {
    n--;
    c++;
  }

  if(n == len-5) 
  {
    if(_wcsnicmp(file, L".part", 5) == 0) return -3;
    fni.israr = true;
    fni.bnl = n+1;
    fni.part = 0;
    fni.ns = 0;
    return 0; // old name schema
  }

  if(len < 6) return -4;
  if(_wcsnicmp(file+n-4, L".part", 5) != 0) 
  {
    fni.israr = true;
    fni.bnl = len-4;
    fni.part = 0;
    fni.ns = 0;
    return 0; // old name schema
  }

  if(c > NXRARPL) return -5; // only accept up to part001

  wchar_t nb[10];
  wcsncpy_s(nb, 10, file+n+1, len-5-n); // -5 length of .part

  fni.israr = true;
  fni.bnl = n+1-5;  // -5 length of .part
  fni.part = _wtoi(nb)-1; // -1 because .part01 is first
  fni.ns = c;

  return c;
}
