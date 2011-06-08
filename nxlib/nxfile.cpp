#include "nxlib.h"
#include "nxfile.h"
#include "nxzip.h"


NXFile::NXFile()
: m_buf(NULL),
  m_pos(NULL),
  m_len(0)
{
}



NXFile::~NXFile()
{
  cleanup();
}



int NXFile::load(const char* path)
{
  int fd = -1;
  if(_sopen_s(&fd, path, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IWRITE)) return R(-1, "open error");
  if(fd == -1) return R(-2, "no file descriptor");
  auto_close acfd(fd);

  return load(fd);
}



int NXFile::load(const wchar_t* path)
{
  int fd = -1;
  if(_wsopen_s(&fd, path, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IWRITE)) return R(-1, "open error");
  if(fd == -1) return R(-2, "no file descriptor");
  auto_close acfd(fd);

  return load(fd);
}



int NXFile::load(int fd)
{
  struct stat st;
  if(fstat(fd, &st)) return R(-1, "stat error");
  size_t dsize = st.st_size;

  auto_free<char> data(dsize+1, true); // +1 for \0
  if(!data) return R(-2, "alloc error");
  if(dsize != _read(fd, data, dsize)) return R(-3, "read error");

  cleanup();
  m_buf = data.release();
  m_len = dsize+1;
  int r = parse1();
  if(r) cleanup();
  return r;
}



int NXFile::parse(const char* data, size_t len)
{
  cleanup();
  m_buf = (char*)NXmemdup(data, len);
  if(!m_buf) return R(-1, "alloc error");
  m_len = len;
  int r = parse1();
  if(r) cleanup();
  return r;
}



int NXFile::parse(char* data, size_t len)
{
  cleanup();
  m_buf = data;
  m_len = len;
  int r = parse1();
  if(r) cleanup();
  return r;
}



int NXFile::parse1()
{
#pragma pack(push)
#pragma pack(1)
  // http://www.gzip.org/zlib/rfc-gzip.html
  struct Gzheader
  {
    unsigned __int8 ID1;
    unsigned __int8 ID2;
    unsigned __int8 CM;
      struct Flags
      {
        unsigned FTEXT : 1;
        unsigned FHCRC : 1;
        unsigned FEXTRA : 1;
        unsigned FNAME : 1;
        unsigned FCOMMENT : 1;
        unsigned RESERVED : 3;
      } FLAGS;
    unsigned __int32 MTIME;
    unsigned __int8 XFL;
    unsigned __int8 OS;
    unsigned __int16 XLEN;
  };

  struct Bzheader
  {
    unsigned __int8 ID1;
    unsigned __int8 ID2;
    unsigned __int8 ID3;
  };
#pragma pack(pop)

  //--- gzip ---
  //-- todo: wasteful, inflate can do all of this ---
  Gzheader* gzheader = (Gzheader*)m_buf;
  if(m_len >= 10+8 && gzheader->ID1 == 0x1f && gzheader->ID2 == 0x8b) // +8 borrow crc/size
  {
    if(NXGZIP.load()) return R(-10, "gzip init error");

    size_t off = 10;
    if(gzheader->FLAGS.FEXTRA) 
    {
      off += gzheader->XLEN + 2;
      if(off > m_len - 8) return R(-11, "gzheader error");
    }
    if(gzheader->FLAGS.FNAME)
    {
      char* p = (char*)memchr(m_buf+off, 0, m_len-off);
      if(p == NULL) return R(-12, "gzheader error");
      off += p - m_buf;
    }
    if(gzheader->FLAGS.FCOMMENT)
    {
      char* p = (char*)memchr(m_buf+off, 0, m_len-off);
      if(p == NULL) return R(-13, "gzheader error");
      off += p - m_buf;
    }
    if(gzheader->FLAGS.FHCRC) off += 2;

    size_t len = (int)m_buf[m_len - 4];
    if(len > 50*m_len) return R(-14, "unrealistic compression");
    len++; // +1 for \0
    auto_free<char> buf(len);
    if(!buf) return R(-15, "alloc error");

    z_stream s;
    memset(&s, 0, sizeof(z_stream));
    s.next_in = (Bytef*)m_buf;
    s.avail_in = m_len;
    if(NXGZIP.inflateInit(&s, 16, ZLIB_VERSION, sizeof(z_stream)) != Z_OK) return R(-16, "gz error");

    s.next_out = (Bytef*)(char*)buf;
    s.avail_out = len-1;
    int r = NXGZIP.inflate(&s, Z_FINISH);
    NXGZIP.inflateEnd(&s);
    if(r != Z_OK) return R(-17, "uncompress error %d", r);

    NXfree(m_buf);
    m_buf = buf.release();
    m_len = len;
  }


  Bzheader* bzheader = (Bzheader*)m_buf;
  if(m_len > 10 && bzheader->ID1 == 'B' && bzheader->ID2 == 'Z' && bzheader->ID3 == 'h')
  {
    if(NXBZIP2.load()) return R(-20, "bz2 init error");

    bz_stream s;
    memset(&s, 0, sizeof(bz_stream));
    s.next_in = m_buf;
    s.avail_in = m_len;
    if(NXBZIP2.decompressInit(&s, 0, 0) != BZ_OK) return R(-21, "bz error");

    size_t len = m_len * NXBZGROW;
    size_t at = 0;
    auto_free<char> buf(len, true);
    if(!buf) return R(-22, "alloc error");
    for(;;)
    {
      s.next_out = buf+at;
      s.avail_out = len-at-1;
      int r = NXBZIP2.decompress(&s);
      if(r == BZ_STREAM_END) break;
      if(r != BZ_OK) return R(-23, "bz error");
      if(len > 100000000) return R(-24, "too much data");
      at = s.total_out_lo32;
      len += m_len * NXBZGROW;
      buf.resize(len, true);
      if(!buf) return R(-25, "alloc error");
    }

    NXBZIP2.decompressEnd(&s);

    NXfree(m_buf);
    m_buf = buf.release();
    m_len = len;
  }

  return parse2();
}



int NXFile::parse2()
{
  char* b = NULL;
  char* t = NULL;
  size_t p = 0;

  m_pos = m_buf;
  for(b = m_buf; *b; b++)
  {
    if(b[0] == '\t' || b[0] == '\n' || b[0] == '\r') 
    {
      b[0] = '\0';
    }
  }


  //--- duplicate separator cleanup ---
  b = m_buf;
  t = m_buf;
  p = 0;
  while(b[0] == '\0' && p < m_len) 
  {
    b++;
    p++;
  }
  while(p < m_len)
  {
    if(b != t)
      t[0] = b[0];

    if(b[0] == '\0' && b[1] == '\0')
    {
      b++;
      p++;
    }
    else
    {
      b++;
      p++;
      t++;
    }
  }
  while(t < b)
  {
    t[0] = '\0';
    t++;
  }

  return 0;
}



 bool NXFile::pair(const char*& key, const char*& value)
 {
   for(;;)
   {
     key = NULL;
     value = NULL;
     if(!m_pos[0] || (size_t)(m_pos - m_buf) >= m_len) return false;
     key = m_pos;
     while(m_pos[0]) m_pos++;
     m_pos++;
     if(!m_pos[0]) return false;
     value = m_pos;
     while(m_pos[0]) m_pos++;
     m_pos++;
     if(strcmp(key, "#") != 0) { break; }
   }
   return true;
 }



const char* NXFile::find(const char* key, const char* value)
{
  m_pos = m_buf;
  const char* k;
  const char* v;
  while(pair(k, v))
  {
    if(strcmp(k, key) == 0)
    {
      if(!value)
        return v;

      if(strcmp(v, value) == 0)
        return v;
    }
  }
  return NULL;
}



void NXFile::cleanup()
{
  NXfree(m_buf); m_buf = NULL;
  m_pos = NULL;
  m_len = 0;
}




int NXFile::seek(size_t pos)
{
  if(pos > m_len) return R(-1, "position out of range");

  m_pos = m_buf + pos;

  return 0;
}
