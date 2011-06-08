#include "nxlib.h"
#include "nxfile.h"
#include "nxhttp.h"
#include <time.h>



NXLFSFileNews::NXLFSFileNews(const char* name, __int64 size, __time32_t time, NXLFSFile* next)
: NXLFSFile(name, time, next),
  m_blocks(0),
  m_bi(NULL)
{
  m_size = size;
  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileNews::NXLFSFileNews(size_t blocks, NXLFSFileNews::Blockinfo* bi, const wchar_t* name, __time64_t time, NXLFSFile* next)
: NXLFSFile(name, time, next),
  m_blocks(blocks),
  m_bi(bi)
{
  m_size = 0;
  for(size_t i = 0; i < m_blocks; i++)
  {
    m_size += m_bi[i].end - m_bi[i].start;
  }

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileNews::NXLFSFileNews(size_t blocks, NXLFSFileNews::Blockinfo* bi, const char* name, __time32_t time, NXLFSFile* next)
: NXLFSFile(name, time, next),
  m_blocks(blocks),
  m_bi(bi)
{
  m_size = 0;
  for(size_t i = 0; i < m_blocks; i++)
  {
    m_size += m_bi[i].end - m_bi[i].start;
  }

  if(NXLFS::i()) NXLFS::i()->addSize(m_size);
}



NXLFSFileNews::~NXLFSFileNews()
{
  NXfree(m_bi); m_bi = NULL;
  m_blocks = 0;

  if(NXLFS::i()) NXLFS::i()->rmSize(m_size);
}



/*virtual*/int NXLFSFileNews::open()
{
  return 0;
}



/*virtual*/int NXLFSFileNews::read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint) 
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + *rlen > m_size)
  {
    *rlen = (size_t)(m_size - offset);
  }
  if(*rlen == 0) return 0;

  if(m_head)
  {
    if(offset < NXNEWSHEAD && offset + *rlen > NXNEWSHEAD)
    {
      printf("info:\thead too small %d vs %lld\n", NXNEWSHEAD, offset + *rlen);
    }
    if(offset + *rlen < NXNEWSHEAD)
    {
      memcpy(buffer, m_head+offset, *rlen);
      return 0;
    }
  }

  if(!m_bi && loadbi()) return R(-2, "loadbi error");

  __int64 pos = offset;
  size_t boffset = 0;
  size_t b;
  for(b = 0; b < m_blocks; b++)
  {
    size_t blen = m_bi[b].end - m_bi[b].start;

    if(pos < blen)
    {
      boffset = (size_t)pos;
      break;
    }
    pos -= blen;
  }
  // todo: if we do everything right that should not happen
  if(b == m_blocks) return R(-3, "block limit reached");


  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-4, "no message cache");

  size_t tg = *rlen;
  char* cbuffer = (char*)buffer;
  bool first = true;
  while(tg)
  {
    // todo: if we do everything right that should not happen
    if(b >= m_blocks) return R(-5, "block limit reached");

    size_t w = min(m_bi[b].end-boffset-m_bi[b].start, tg);
    tg -= w;

    if(first && (readahead || tg))
    {
      first = false;
      const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
      if(!cfg) return R(-6, "no config");
      
      size_t left;
      if(readahead)
        left = (size_t)(cfg->maxmem() *.8);
      else
        left = tg;

      //printf("readahead: left %d\n", left);
      size_t ba;
      for(ba = b+1; ba < m_blocks; ba++)
      {
        if(left < m_bi[ba].end - m_bi[ba].start) break;
        left -= m_bi[ba].end - m_bi[ba].start;
        if(cache->aget(m_bi[ba].msgid) <= 0) break;
      }
      //printf("readahead: blocks %d..%d size %d left %d\n", b+1, pa, m_bi[b].size, left);

      //--- reached end of file, and got a next file hint... ---
      if(ba == m_blocks && rahint)
      {
        //--- ...and its a news file again (cant imagine how it wouldnt...) ---
        NXLFSFileNews* n = dynamic_cast<NXLFSFileNews*>(rahint);
        if(n)
        {
          for(ba = 0; ba < n->m_blocks; ba++)
          {
            if(left < n->m_bi[ba].end - n->m_bi[ba].start) break;
            left -= n->m_bi[ba].end - n->m_bi[ba].start;
            if(cache->aget(n->m_bi[ba].msgid) == 0) break;
          }
        }
      }
    }

/*
    const NXPart* d = cache->get(m_bi[b].msgid);
    if(!d) return R(-7, "message data not received");

    memcpy(cbuffer, &d->data+boffset+m_bi[b].start, w);
*/
    if(cache->get(m_bi[b].msgid, cbuffer, boffset+m_bi[b].start, w)) R(-7, "message data not received");


    cbuffer += w;
    b++;
    boffset = 0;
  }

  return 0;
}


/*
  pretend read

  return:
    < 0 ... error
      0 ... message should be there
    > 1 ... message status indicates future error
*/
/*virtual*/int NXLFSFileNews::pread(void* buffer, size_t blen, size_t* rlen, __int64 offset) 
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + *rlen > m_size)
  {
    *rlen = (size_t)(m_size - offset);
  }
  if(*rlen == 0) return 0;

  if(m_head)
  {
    if(offset < NXNEWSHEAD && offset + *rlen > NXNEWSHEAD)
    {
      printf("info:\thead too small %d vs %lld\n", NXNEWSHEAD, offset + *rlen);
    }
    if(offset + *rlen < NXNEWSHEAD)
    {
      memcpy(buffer, m_head+offset, *rlen);
      return 0;
    }
  }

  if(!m_bi && loadbi()) return R(-2, "loadbi error");

  __int64 pos = offset;
  size_t boffset = 0;
  size_t b;
  for(b = 0; b < m_blocks; b++)
  {
    size_t blen = m_bi[b].end - m_bi[b].start;

    if(pos < blen)
    {
      boffset = (size_t)pos;
      break;
    }
    pos -= blen;
  }
  // todo: if we do everything right that should not happen
  if(b == m_blocks) return R(-3, "block limit reached");


  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-4, "no message cache");

  size_t tg = *rlen;
  char* cbuffer = (char*)buffer;
  bool first = true;
  while(tg)
  {
    // todo: if we do everything right that should not happen
    if(b >= m_blocks) return R(-5, "block limit reached");

    size_t w = min(m_bi[b].end-boffset-m_bi[b].start, tg);
    tg -= w;

    if(m_bi[b].status) return 1;

    cbuffer += w;
    b++;
    boffset = 0;
  }

  return 0;
}



int NXLFSFileNews::aread(size_t rlen, __int64 offset) 
{
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + rlen > m_size)
  {
    rlen = (size_t)(m_size - offset);
  }
  if(rlen == 0) return 0;

  if(m_head && offset + rlen < NXNEWSHEAD)
    return 0;

  if(!m_bi && loadbi()) return R(-2, "loadbi error");

  __int64 pos = offset;
  size_t boffset = 0;
  size_t b;
  for(b = 0; b < m_blocks; b++)
  {
    size_t blen = m_bi[b].end - m_bi[b].start;

    if(pos < blen)
    {
      boffset = (size_t)(pos - m_bi[b].start);
      break;
    }
    pos -= blen;
  }
  // todo: if we do everything right that should not happen
  if(b == m_blocks) return R(-3, "block limit reached");


  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-4, "no message cache");

  size_t tg = rlen;
  while(tg)
  {
    // todo: if we do everything right that should not happen
    if(b >= m_blocks)  return R(-5, "block limit reached");

    size_t w = min(m_bi[b].end-boffset-m_bi[b].start, tg);

    int cid = cache->aget(m_bi[b].msgid);
    if(cid == 0) return 0; // all busy

    tg -= w;
    b++;
    boffset = 0;
  }

  return 0;
}



int NXLFSFileNews::blockinfo(NXLFSFileNews::Blockinfo*& bi, size_t rlen, __int64 offset) const
{
  bi = NULL;
  if(offset > m_size) return R(-1, "offset out of range (%lld/%lld)", offset, m_size);
  if(offset + rlen > m_size)
  {
    rlen = (size_t)(m_size - offset);
  }
  if(rlen == 0) return 0;

  if(!m_bi && const_cast<NXLFSFileNews*>(this)->loadbi()) return R(-2, "loadbi error");

  __int64 pos = offset;
  size_t boffset = 0;
  size_t b;
  for(b = 0; b < m_blocks; b++)
  {
    size_t blen = m_bi[b].end - m_bi[b].start;

    if(pos < blen)
    {
      boffset = (size_t)(pos - m_bi[b].start);
      break;
    }
    pos -= blen;
  }
  // todo: if we do everything right that should not happen
  if(b == m_blocks) return R(-3, "block limit reached");


  //--- round 1: count blocks ---
  int bic = 0;
  size_t tg = rlen;
  size_t boffset0 = boffset;
  size_t b0 = b;
  while(tg)
  {
    // todo: if we do everything right that should not happen
    if(b >= m_blocks) 
    {
      return R(-4, "block limit reached");
    }

    size_t w = min(m_bi[b].end-boffset-m_bi[b].start, tg);
    bic++;
    tg -= w;
    b++;
    boffset = 0;
  }


  bi = (Blockinfo*)NXrealloc(bi, sizeof(Blockinfo) * bic);
  if(!bi) return R(-5, "realloc error");
  memset(bi, 0, sizeof(Blockinfo) * bic);


  //--- round 2: collect ---
  bic = 0;
  tg = rlen;
  boffset = boffset0;
  b = b0;
  while(tg)
  {
    size_t w = min(m_bi[b].end-boffset-m_bi[b].start, tg);

    bi[bic].start = boffset + m_bi[b].start;
    bi[bic].end = boffset + m_bi[b].start + w;
    bi[bic].size = m_bi[b].size;
    if(strcpy_s(bi[bic].msgid, NXMAXMSGID, m_bi[b].msgid)) 
    {
      NXfree(bi); bi = NULL;
      return R(-6, "buffer error");
    }

    bic++;
    tg -= w;
    b++;
    boffset = 0;
  }

  return bic;
}



/*
int NXLFSFileNews::blockinfo(NXLFSFileNews::sblockinfo& bi, int block) const
{
  if(block > m_blocks) return R(1);

  size_t pos = 0;
  for(int b = 0; b < block; b++)
  {
    size_t start = m_start ? m_start[b] : 0;
    size_t end = m_end ? m_end[b] : m_blocksize[b];
    size_t blen = end - start;
    pos += blen;
  }

  bi.bsize = m_blocksize[block];
  bi.bstart = (m_start ? m_start[block] : 0);
  bi.bend = (m_end ? m_end[block] : m_blocksize[block]);
  bi.blen = bi.bend - bi.bstart;
  bi.fstart = pos + bi.bstart;
  bi.fend = pos + bi.bend;

  return 0;
}
*/


int NXLFSFileNews::close() 
{
  return 0;
}



/*static*/int NXLFSFileNews::loadnxi(const char* path, NXLFSFile*& next)
{
  NXFile nxi;
  if(nxi.load(path)) return R(-1, "nxi load error");


  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();

  char fname[MAX_PATH] = "";
  __int64 fsize = -1;
  __time32_t ftime = -1;
  size_t blocks = -1;
  size_t bsize = -1;
  auto_free<NXLFSFileNews::Blockinfo> bi;

  const char* k = NULL;
  const char* v = NULL;
  for(;;)
  {
    bool ok = nxi.pair(k, v);

    if(!ok || strcmp(k, "n") == 0 && fname[0])
    {
      if(fname[0] == '\0') return R(-91, "file name missing");
      if(fsize == -1) return R(-92, "file size missing");
      if(ftime == -1) return R(-93, "file time missing");
      if(blocks == -1) return R(-94, "block end missing");
      
      __int64 fsizec = 0;
      for(size_t b = 0; b < blocks; b++)
        fsizec += bi[b].end - bi[b].start;
      if(fsize != fsizec) return R(-95, "size mismatch");

      if(!NXfextmchk(fname, cfg->nzbextfilter()) && 
         !NXfnammchk(fname, cfg->nzbnamefilter()))
      {
        next = new NXLFSFileNews(blocks, bi.release(), fname, ftime, next);
      }

      fname[0] = '\0';
      fsize = -1;
      ftime = -1;
      blocks = -1;
      bsize = -1;

      if(!ok) break;
    }

    if(strcmp(k, "n") == 0)
    {
      if(strcpy_s(fname, MAX_PATH, v)) return R(-11, "file name limit reached");
      continue;
    }

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

    if(strcmp(k, "e") == 0)
    {
      bsize = atol(v);
      if(bsize <= 0) return R(-41, "block end value error");
      if(bsize == LONG_MAX) return R(-42, "block end value error");
      continue;
    }

    if(strcmp(k, "b") == 0)
    {
      if(fname[0] == '\0') return R(-51, "file name missing");
      if(fsize == -1) return R(-52, "file size missing");
      if(ftime == -1) return R(-53, "file time missing");
      if(bsize == -1) return R(-54, "block end missing");

      blocks = atol(v);
      if(blocks <= 0) return R(-61, "blocks value error");
      if(blocks == LONG_MAX) return R(-62, "blocks value error");


      size_t pos = nxi.tell();
      size_t blocks = 0;
      while(nxi.pair(k, v))
      {
        if(strcmp(k, "i") == 0)
        {
          blocks++;
          continue;
        }
        if(strcmp(k, "n") == 0)
        {
          break;
        }
      }
      if(nxi.seek(pos)) return R(-63, "seek error");


      size_t block = 0;
      bi.get(blocks, true); // +1 because we could be lied to
      NXLFSFileNews::Blockinfo* cbi = bi;
      bool newb = false;
      for(;;)
      {
        ok = nxi.pair(k, v);
        if(!ok) break;

        if(strcmp(k, "i") == 0)
        {
          if(newb) 
          {
            if(cbi->start >= cbi->end) return R(-101, "block range error");
            if(cbi->start >= cbi->size) return R(-102, "block range error");
            if(cbi->end > cbi->size) return R(-103, "block range error");
            if(block == blocks) return R(-104, "too many blocks");

            cbi++;
            block++;
          }
          newb = true;

          if(strcpy_s(cbi->msgid, NXMAXMSGID, v)) return R(-105, "msgid too long");
          cbi->size = bsize;
          cbi->start = 0;
          cbi->end = bsize;
          continue;
        }

        if(strcmp(k, "s") == 0)
        {
          cbi->start = atol(v);
          if(cbi->start < 0) return R(-111, "block start value error");
          if(cbi->start == LONG_MAX) return R(-112, "block start value error");
          continue;
        }

        if(strcmp(k, "e") == 0)
        {
          cbi->size = cbi->end = atol(v);
          if(cbi->end <= 0) return R(-121, "block end value error");
          if(cbi->end == LONG_MAX) return R(-122, "block end value error");
          continue;
        }

        if(strcmp(k, "c") == 0)
        {
          long l = atol(v);
          if(l < 0) return R(-131, "block status value error");
          if(l == LONG_MAX) return R(-132, "block status value error");
          if(l > 3) return R(-133, "block status value error");
          cbi->status = (__int8)l;
          continue;
        }

        break;
      }

      if(block != blocks-1) 
        return R(-199, "not enough blocks");

      continue;
    }

    return R(-99, "unexpected tag");
  }

  return 0;
}



/*static*/int NXLFSFileNews::loadnxh(const char* path, const char* pathnxi, NXLFSFile*& next)
{
  NXFile nxh;
  if(nxh.load(path)) return R(-1, "nxfile load error");

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();

  char fname[MAX_PATH] = "";
  __int64 fsize = -1;
  __time32_t ftime = -1;

  const char* k = NULL;
  const char* v = NULL;
  for(;;)
  {
    bool ok = nxh.pair(k, v);
    
    //--- empty nxh, maybe nothing valid was found ---
    if(!ok && !fname[0]) break;
      
    if(!ok || strcmp(k, "n") == 0)
    {
      if(!ok || fname[0])
      {
        if(!fname[0]) return R(-11, "no file name");
        if(fsize == -1) return R(-12, "no file size");
        if(ftime == -1) return R(-13, "no file time");

        if(!NXfextmchk(fname, cfg->nzbextfilter()) && 
           !NXfnammchk(fname, cfg->nzbnamefilter()))
        {
          next = new NXLFSFileNewsNXI(pathnxi, fname, fsize, ftime, next);
        }
        
        fsize = -1;
        ftime = -1;

        if(!ok) return 0;
      }
      if(strcpy_s(fname, MAX_PATH, v)) return R(-14, "buffer error");
      continue;
    }

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
  }

  return 0;
}



void NXLFSFileNews::savenxibi(FILE* out, const NXLFSFileNews::Blockinfo* bi, size_t blocks, size_t blocksize) const
{
  for(size_t i = 0; i < blocks; i++)
  {
    fprintf(out, "i\t%s", bi[i].msgid);
    if(bi[i].start) 
      fprintf(out, "\ts\t%d", bi[i].start);
    if(bi[i].end != blocksize) 
      fprintf(out, "\te\t%d", bi[i].end);
    if(bi[i].status) 
      fprintf(out, "\tc\t%d", bi[i].status);
    fprintf(out, "\n");
  }
}



int NXLFSFileNews::savenxi(FILE* out) const
{
  if(!m_bi) return R(-1, "no blockinfo");
  
  size_t blocksize = commonbs(m_bi, m_blocks);
  savenxihead(out, this, m_blocks, blocksize);
  savenxibi(out, m_bi, m_blocks, blocksize);
  
  return 0;
}



void NXLFSFileNews::savenxihead(FILE* out, const NXLFSFile* file, size_t blocks, size_t blocksize) const
{
  fprintf(out, "n\t%ws\n", file->name());
  fprintf(out, "s\t%lld\n", file->size());
  fprintf(out, "t\t%d\n", NXtimet64to32(file->time()));
  if(blocks) fprintf(out, "e\t%d\n", blocksize);
  if(blocksize) fprintf(out, "b\t%d\n", blocks);
}



size_t NXLFSFileNews::commonbs(const NXLFSFileNews::Blockinfo* bi, size_t blocks) const
{
  size_t be[10];  // block end
  size_t bec[10]; // block end count
  memset(be, 0, sizeof(be));
  memset(bec, 0, sizeof(bec));

  //--- block end usage ---
  for(size_t i = 0; i < blocks; i++)
  {
    for(int j = 0; j < 10; j++)
    {
      if(be[j] == bi[i].end)
      {
        bec[j]++;
        break;
      }
      if(be[j] == 0)
      {
        bec[j] = 1;
        be[j] = bi[i].end;
        break;
      }
    }
  }
  int i = 0;
  for(int j = 1; j < 10; j++)
  {
    if(bec[j] > bec[i]) i = j;
  }

  return be[i];
}



int NXLFSFileNews::parsebi(NXFile& nxi)
{
  char fname[MAX_PATH];
  if(sprintf_s(fname, MAX_PATH, "%ws", name()) == -1) return R(-3, "buffer error");
  if(!nxi.find("n", fname)) return R(-4, "file not found");

  size_t bsize = -1;
  size_t blocks = -1;
  auto_free<NXLFSFileNews::Blockinfo> bi;

  const char* k = NULL;
  const char* v = NULL;
  while(nxi.pair(k, v))
  {
    //--- todo: could do sanity check on size and time... ---

    if(strcmp(k, "e") == 0)
    {
      bsize = atol(v);
      if(bsize <= 0) return R(-111, "block end value error");
      if(bsize == LONG_MAX) return R(-112, "block end value error");
      continue;
    }

    if(strcmp(k, "b") == 0)
    {
      blocks = atol(v);
      if(blocks <= 0) return R(-121, "blocks value error");
      if(blocks == LONG_MAX) return R(-122, "blocks value error");

      size_t pos = nxi.tell();
      size_t cblocks = 0;
      while(nxi.pair(k, v))
      {
        if(strcmp(k, "i") == 0)
        {
          cblocks++;
          continue;
        }
        if(strcmp(k, "n") == 0)
        {
          break;
        }
      }
      if(blocks != cblocks) return R(-123, "block count error");
      if(nxi.seek(pos)) return R(-124, "seek error");

      size_t block = 0;
      bi.get(blocks, true);
      NXLFSFileNews::Blockinfo* cbi = bi;
      bool newb = false;
      while(nxi.pair(k, v))
      {
        if(strcmp(k, "i") == 0)
        {
          if(newb) 
          {
            if(cbi->start >= cbi->end) return R(-201, "block range error");
            if(cbi->start >= cbi->size) return R(-202, "block range error");
            if(cbi->end > cbi->size) return R(-203, "block range error");
            if(block == blocks) return R(-204, "too many blocks");

            cbi++;
            block++;
          }
          newb = true;

          if(strcpy_s(cbi->msgid, NXMAXMSGID, v)) return R(-205, "buffer error");
          cbi->size = bsize;
          cbi->start = 0;
          cbi->end = bsize;
          continue;
        }

        if(strcmp(k, "s") == 0)
        {
          cbi->start = atol(v);
          if(cbi->start < 0) return R(-211, "block start value error");
          if(cbi->start == LONG_MAX) return R(-212, "block start value error");
          continue;
        }

        if(strcmp(k, "e") == 0)
        {
          cbi->size = cbi->end = atol(v);
          if(cbi->end <= 0) return R(-221, "block end value error");
          if(cbi->end == LONG_MAX) return R(-222, "block end value error");
          continue;
        }

        if(strcmp(k, "c") == 0)
        {
          long l = atol(v);
          if(l < 0) return R(-231, "block status value error");
          if(l == LONG_MAX) return R(-232, "block status value error");
          if(l > 3) return R(-233, "block status value error");
          cbi->status = (__int8)l;
          continue;
        }

        break;
      }

      if(block != blocks-1) 
        return R(-131, "not enough blocks");

      __int64 fsizec = 0;
      for(size_t b = 0; b < blocks; b++)
        fsizec += bi[b].end - bi[b].start;
      if(m_size != fsizec) return R(-132, "size mismatch");

      m_bi = bi.release();
      m_blocks = blocks;

      return 0;
    }
  }

  return R(-199, "got lost");
}



NXLFSFileNewsNXI::NXLFSFileNewsNXI(const char* nxi, const char* name, __int64 size, __time32_t time, NXLFSFile* next)
: NXLFSFileNews(name, size, time, next),
  m_nxi(NXstrdup(nxi))
{
}



NXLFSFileNewsNXI::~NXLFSFileNewsNXI()
{
  NXfree(m_nxi); m_nxi = NULL;
}



/*virtual*/int NXLFSFileNewsNXI::loadbi()
{
  if(m_bi) return 0;

  NXFile nxi;
  if(nxi.load(m_nxi)) return R(-1, "nxi load error");

  return parsebi(nxi);
}  



NXLFSFileNewsHTTP::NXLFSFileNewsHTTP(const char* nxi, const char* name, __int64 size, __time32_t time, NXLFSFile* next)
: NXLFSFileNews(name, size, time, next),
  m_nxi(NXstrdup(nxi))
{
}



NXLFSFileNewsHTTP::~NXLFSFileNewsHTTP()
{
  NXfree(m_nxi); m_nxi = NULL;
}



/*virtual*/int NXLFSFileNewsHTTP::loadbi()
{
  if(m_bi) return 0;

  NXHTTP req;
  if(req.get(m_nxi)) return R(-1, "http get error");

  NXFile nxi;
  if(nxi.parse(req.body(), req.bodysize())) return R(-2, "nxi parse error");

  return parsebi(nxi);
}



NXLFSFileNewsNNTP::NXLFSFileNewsNNTP(const char* msgid, const char* name, __int64 size, __time32_t time, NXLFSFile* next)
: NXLFSFileNews(name, size, time, next),
  m_msgid(NXstrdup(msgid))
{
}



NXLFSFileNewsNNTP::~NXLFSFileNewsNNTP()
{
  NXfree(m_msgid); m_msgid = NULL;
}



/*virtual*/int NXLFSFileNewsNNTP::loadbi()
{
  if(m_bi) return 0;

  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-1, "no cache");
  
  NXPart* p = NULL;
  if(cache->get(m_msgid, p)) return R(-2, "get message error");
  auto_free<NXPart> afp(p);

  NXFile nxi;
  if(nxi.parse(&p->data, p->len)) return R(-3, "nxi parse error");

  return parsebi(nxi);
}
