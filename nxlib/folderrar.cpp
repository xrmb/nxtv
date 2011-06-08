#include "nxlib.h"

#include "crc32.h"



NXLFSFolderRAR::NXLFSFolderRAR(NXLFSFile* file, const wchar_t* name, __time64_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next),
  m_file(file),
  m_err(NULL)
{
}



NXLFSFolderRAR::~NXLFSFolderRAR()
{
  delete m_file; m_file = NULL;
  NXfree(m_err); m_err = NULL;
}



/*virtual*/int NXLFSFolderRAR::build()
{
  if(m_build) return m_buildr;
  m_build = true;

  m_buildr = buildr();
  if(m_buildr)
  {
    m_files = m_file;
    m_file = NULL;
  }
  return m_buildr;
}



int NXLFSFolderRAR::buildr()
{
  cleanff();


  CRC32 crc;
  for(NXLFSFile* ff = m_file; ff; ff = ff->next())
  {
    crc.hash(ff->name(), wcslen(ff->name())*sizeof(wchar_t));
  }

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  if(!cfg) return R(-1, "no config");

  char pathnxi[MAX_PATH];
  char pathnxh[MAX_PATH];
  char patherr[MAX_PATH];
  if(cfg->cache())
  {
    if(sprintf_s(patherr, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "rar", (crc.get() & 0xF0000000) >> 28, crc.get(), "err") == -1) return R(-11, "buffer error");
    if(_access(patherr, 0x04) == 0)
    {
      struct _stat fs;
      if(_stat(patherr, &fs)) return R(-12, "stat error");
      m_files = new NXLFSFileFS(patherr, fs.st_size, L"!errors.txt", fs.st_mtime, m_files);
      return 0;
    }

    if(sprintf_s(pathnxi, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "rar", (crc.get() & 0xF0000000) >> 28, crc.get(), "nxi") == -1) return R(-13, "buffer error");
    if(sprintf_s(pathnxh, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "rar", (crc.get() & 0xF0000000) >> 28, crc.get(), "nxh") == -1) return R(-14, "buffer error");
    if(_access(pathnxi, 0x04) == 0)
    {
      int r = NXLFSFileNews::loadnxh(pathnxh, pathnxi, m_files);
      if(r)
      {
        cleanff();
        _unlink(pathnxi); // ERR
        _unlink(pathnxh); // ERR
        char msg[MAX_PATH+50];
        sprintf_s(msg, MAX_PATH+50, "loadnxh error %d in %ws", r, pathnxh);
        m_files = new NXLFSFileText(msg, L"!message.txt", m_time, m_files);
      }
      if(r == 0)
      {
        delete m_file; m_file = NULL;
        return 0;
      }
    }
  }


  int r = buildrr();

  if(r > 0)
  {
    FILE* fh = NULL;
    if(fopen_s(&fh, patherr, "w")) return R(-21, "open error");
    if(!fh) return R(-22, "open error");
    if(m_file)
      fprintf(fh, "%ws:\n", m_file->name());
    fprintf(fh, "%s", m_err);
    fclose(fh);

    struct _stat fs;
    if(_stat(patherr, &fs)) return R(-23, "stat error");
    m_files = new NXLFSFileFS(patherr, fs.st_size, L"!errors.txt", fs.st_mtime, m_files);
  }


  if(cfg->cache())
  {
    savenxh(pathnxh);
    savenxi(pathnxi);
  }

  return 0;
}



int NXLFSFolderRAR::buildrr()
{
#pragma pack(push)
#pragma pack(1)
  struct 
  {
    unsigned __int16 HEAD_CRC;
    unsigned __int8  HEAD_TYPE;
    unsigned __int16 HEAD_FLAGS;
    unsigned __int16 HEAD_SIZE;
  } mh;

  struct 
  {
    unsigned __int16 HEAD_CRC;
    unsigned __int8  HEAD_TYPE;
    unsigned __int16 HEAD_FLAGS;
    unsigned __int16 HEAD_SIZE;
    unsigned __int16 RESERVED1;
    unsigned __int32 RESERVED2;
  } ah;

  struct
  {
    unsigned __int16 HEAD_CRC;
    unsigned __int8  HEAD_TYPE;
    unsigned __int16 HEAD_FLAGS;
    unsigned __int16 HEAD_SIZE;
    unsigned __int32 PACK_SIZE;
    unsigned __int32 UNP_SIZE;
    unsigned __int8  HOST_OS;
    unsigned __int32 FILE_CRC;
    unsigned __int32 FTIME;
    unsigned __int8  UNP_VER;
    unsigned __int8  METHOD;
    unsigned __int16 NAME_SIZE;
    unsigned __int32 ATTR;
  } fh;

  struct
  {
    unsigned __int32 HIGH_PACK_SIZE;
    unsigned __int32 HIGH_UNP_SIZE;
  } hh;
#pragma pack(pop)

  cleanff();


  __int64 fsz = -1;
  char pfn[MAX_PATH] = "";
  NXLFSFileRAR::Piece* pieces = NULL;

  auto_deletev<NXLFSFileRAR::Piece> ad_pieces(&pieces);
  auto_deletev<NXLFSFile> ad_files(&m_files);


  for(NXLFSFile* ff = m_file; ff; ff = ff->next())
  {
    //printf(L"processing rar file %ws\n", ff->name());

    NXNNTPPool* pool = NXNNTPPool::i();
    if(!pool) return R(-1, "no message pool");
    int ic = pool->idlecount();
    for(NXLFSFile* ffa = ff->next(); ic && ffa; ffa = ffa->next(), ic--)
    {
      if(ffa->aread(sizeof(mh) + sizeof(ah) + sizeof(fh), 0)) return R(-2, "read error");
    }

    char fn[MAX_PATH];
    //wchar_t fnu[MAX_PATH];
    size_t read;
    size_t read2;
    __int64 pos = 0;

    memset(&mh, 0, sizeof(mh));
    memset(&ah, 0, sizeof(ah));
    memset(&fh, 0, sizeof(fh));
    memset(&hh, 0, sizeof(hh));

    if(ff->open()) return R(-3, "open error");
    auto_close ac_ff(ff);

    //--- marker header first ---
    read = read2 = sizeof(mh);
    if(ff->read(&mh, sizeof(mh), &read, pos, 0)) return R(-11, "read error");
    if(read != read2) return R(-12, "read error");
    pos += read;

    if(mh.HEAD_CRC    != 0x6152)  return RE(13, "header error");
    if(mh.HEAD_TYPE   != 0x72)    return RE(14, "header error");
    if(mh.HEAD_FLAGS  != 0x1a21)  return RE(15, "header error");
    if(mh.HEAD_SIZE   != 0x0007)  return RE(16, "header error");

    //--- archive header next ---
    read = read2 = sizeof(ah);
    if(ff->read(&ah, sizeof(ah), &read, pos, 0)) return R(-21, "read error");
    if(read != read2) return R(-22, "read error");
    pos += read;

    if(ah.HEAD_TYPE != 0x73) return RE(23, "header error (0x%02X)", ah.HEAD_TYPE);
    // todo: if((ah.HEAD_FLAGS & 0x0010) && m_ns == 0) return R(__LINE__);


    //--- blocks, blocks, blocks... ---
    for(;;)
    {
      __int64 bpos = pos;
      read = read2 = sizeof(mh);
      if(ff->read(&mh, sizeof(mh), &read, pos, 0)) return R(-31, "read error");
      if(read != read2) return R(-32, "read error");

      unsigned __int32 ADD_SIZE = 0;
      if(mh.HEAD_FLAGS & 0x8000)
      {
        read = read2 = sizeof(ADD_SIZE);
        if(ff->read(&ADD_SIZE, sizeof(ADD_SIZE), &read, pos+sizeof(mh), 0)) return R(-41, "read error");
        if(read != sizeof(ADD_SIZE)) return R(-42, "read error");
        //pos += read;
      }

      switch(mh.HEAD_TYPE)
      {
      case 0x74:
        read = read2 = sizeof(fh);
        if(ff->read(&fh, sizeof(fh), &read, pos, 0)) return R(-51, "read error");
        if(read != read2) return R(-52, "read error");
        pos += read;

        //if(fh.METHOD != 0x30) return RE(53, "no store method (0x%02x)", fh.METHOD);
    

        if(fh.HEAD_FLAGS & 0x0100) 
        {
          read = read2 = sizeof(hh);
          if(ff->read(&hh, sizeof(hh), &read, pos, 0)) return R(-61, "read error");
          if(read != read2) return R(-62, "read error");
          pos += read;

          //--- piece len is only 32bit ---
          if(hh.HIGH_PACK_SIZE) return R(63, "huge block");
        }
        if(fh.HEAD_FLAGS & 0x0004) return RE(71, "encrypted");
        if((fh.HEAD_FLAGS & 0x00E0) == 0x00E0) return RE(72, "directory");
        //if(fh.HEAD_FLAGS & 0x0200) return RE(73, "unicode filename"); 
        if(fh.HEAD_FLAGS & 0x0400) return RE(74, "salt");  
        if(fh.HEAD_FLAGS & 0x0800) return RE(75, "version flag");  
        if(fh.NAME_SIZE+1 > MAX_PATH) return RE(76, "filename too long");

        //--- the unicode name encoding is unclear, the fn buffer needs to be much bigger for files with lots of unicode chars... ---
        read = read2 = fh.NAME_SIZE;
        if(ff->read(fn, MAX_PATH, &read, pos, 0)) return R(-81, "read error");
        if(read != read2) return R(-82, "read error");
        pos += read;
        fn[fh.NAME_SIZE] = '\0';
        //--- todo: unicode decoding not finished ---
        if(fh.HEAD_FLAGS & 0x0200)
        {
          /*
          read = strlen(fn)+1;
          swprintf_s(fnu, L"%hs", fn);
          //--- here starts the unicode data
          fn[read];
          */
        }

        if(strchr(fn, '/') || strchr(fn, '\\')) return RE(85, "directory");

        if(pfn[0] && strcmp(pfn, fn) != 0)
        {
          if(fh.METHOD == 0x30)
          {
            if(pieces->size() != fsz) return RE(91, "size mismatch %lld vs %lld", pieces->size(), fsz);
            m_files = new NXLFSFileRAR(pieces, pfn, NXdostimeto32(fh.FTIME), m_files);
            ad_pieces.reset(false);
          }
          else
          {
            if(strcat_s(pfn, MAX_PATH, "!")) return RE(92, "buffer error");
            _strupr(pfn);
            m_files = new NXLFSFileText("file is compressed", pfn, NXdostimeto32(fh.FTIME), m_files);
            ad_pieces.reset(true);
          }
          pfn[0] = '\0';
          fsz = -1;
        }

        //--- in solid archives the size changes after the first part, dont really know what the value now means ---
        //--- note: && !pfn is due to buggy rars?
        if((fh.HEAD_FLAGS & 0x0010) == 0 && !pfn[0] || fsz == -1) fsz = hh.HIGH_UNP_SIZE*0x100000000LL + fh.UNP_SIZE;

        if(!pfn[0]) strncpy(pfn, fn, MAX_PATH); // ERR (not needded)

        new NXLFSFileRAR::Piece(ff, bpos + fh.HEAD_SIZE, fh.PACK_SIZE, pieces);
        //printf("%ws: data block %s at %lld length %d 0x%08llX-0x%08llX\n", ff->name(), pfn, bpos + fh.HEAD_SIZE, fh.PACK_SIZE, bpos + fh.HEAD_SIZE, bpos + fh.HEAD_SIZE + fh.PACK_SIZE);

        //--- continues next volume, lets us skip some blocks coming ---
        if(fh.HEAD_FLAGS & 0x0002) 
          pos = ff->size();
        else
          pos = bpos + fh.HEAD_SIZE + hh.HIGH_PACK_SIZE*0x100000000LL + fh.PACK_SIZE;
        break;

      default:
        pos = bpos + mh.HEAD_SIZE + ADD_SIZE;
        //printf("%ws: block 0x%02X at %lld length %d 0x%08llX-0x%08llX\n", ff->name(), mh.HEAD_TYPE, bpos + fh.HEAD_SIZE, fh.HEAD_SIZE + ADD_SIZE, bpos + fh.HEAD_SIZE, bpos + fh.HEAD_SIZE + fh.PACK_SIZE + ADD_SIZE);
      }

      if(pos == ff->size()) break; // next files in set
      if(pos > ff->size()) return RE(91, "offset out of range (%lld/%lld)", pos, ff->size());
    }
  }

  if(!pieces || !pfn[0]) return RE(101, "no data");

  if(fh.HEAD_FLAGS & 0x0002) return RE(102, "volume missing after %s", pfn);

  if(fh.METHOD == 0x30)
  {
    if(pieces->size() != fsz) return RE(103, "size mismatch %lld vs %lld", pieces->size(), fsz);
    m_files = new NXLFSFileRAR(pieces, pfn, NXdostimeto32(fh.FTIME), m_files);
    ad_pieces.reset(false);
  }
  else
  {
    if(strcat_s(pfn, MAX_PATH, "!")) return RE(104, "buffer error");
    _strupr(pfn);
    m_files = new NXLFSFileText("file is compressed", pfn, NXdostimeto32(fh.FTIME), m_files);
    ad_pieces.reset(true);
  }

  ad_files.release();

  return 0;
}



int NXLFSFolderRAR::savenxi(const char* path)
{
  const NXLFSFileRAR* fr = NULL;
  const NXLFSFileNews* fn = NULL;

  /*
  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    fr = dynamic_cast<const NXLFSFileRAR*>(file);
    if(!fr) return 0;
    for(const NXLFSFileRAR::Piece* p = fr->pieces(); p; p = p->n)
    {
      fn = dynamic_cast<const NXLFSFileNews*>(p->f);
      if(!fn) return 0;
    }
  }
  */

  FILE* out = NULL;
  auto_close acout;

  if(fopen_s(&out, path, "wb")) return R(-1, "open error");
  if(out == NULL) return R(-2, "open error");
  acout.set(out);

  bool first = true;
  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    fr = dynamic_cast<const NXLFSFileRAR*>(file);
    if(!fr) continue;

    NXLFSFileNews::Blockinfo* bi;
    int bit = 0;
    size_t cbs;

    //--- first run, count blocks ---
    const NXLFSFileNews* fn1 = NULL;
    for(const NXLFSFileRAR::Piece* p = fr->pieces(); p; p = p->n)
    {
      fn = dynamic_cast<const NXLFSFileNews*>(p->f);
      if(!fn) continue;

      if(!fn1) fn1 = fn;

      int bic = fn->blockinfo(bi, p->len, p->pos);
      auto_free<NXLFSFileNews::Blockinfo> afbi(bi);
      if(bic <= 0) 
        return R(-3, "blockinfo error");

      if(bit == 0)
        cbs = fn->commonbs(bi, bic);
      bit += bic;
    }
    if(!fn1) continue;

    if(first)
    {
      fprintf(out, "#\tderived from %ws\n", fn1->name());
      first = false;
    }
    fn1->savenxihead(out, file, bit, cbs);

    //--- second run, write it out ---
    for(const NXLFSFileRAR::Piece* p = fr->pieces(); p; p = p->n)
    {
      fn = dynamic_cast<const NXLFSFileNews*>(p->f);
      int bic = fn->blockinfo(bi, p->len, p->pos);
      fn1->savenxibi(out, bi, bic, cbs);
      NXfree(bi);
    }
  }

  if(first)
  {
    fprintf(out, "#\tderived from %ws\n", name());
    fprintf(out, "#\tno usabled files found\n", name());
  }

  fclose(out);

  return 0;
}



int NXLFSFolderRAR::savenxh(const char* path)
{
  const NXLFSFileRAR* fr = NULL;
  const NXLFSFileNews* fn = NULL;

  /*
  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    fr = dynamic_cast<const NXLFSFileRAR*>(file);
    if(!fr) return 0;
    for(const NXLFSFileRAR::Piece* p = fr->pieces(); p; p = p->n)
    {
      fn = dynamic_cast<const NXLFSFileNews*>(p->f);
      if(!fn) return 0;
    }
  }
  */

  FILE* out = NULL;
  if(fopen_s(&out, path, "wb")) return R(-2, "open error");
  if(out == NULL) return R(-3, "open error");

  bool first = true;
  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    fr = dynamic_cast<const NXLFSFileRAR*>(file);
    if(!fr) continue;

    //--- first run, count blocks ---
    const NXLFSFileNews* fn1 = NULL;
    for(const NXLFSFileRAR::Piece* p = fr->pieces(); p; p = p->n)
    {
      fn = dynamic_cast<const NXLFSFileNews*>(p->f);
      if(!fn) continue;

      fn1 = fn;
      break;
    }
    if(!fn1) continue;


    if(first)
    {
      fprintf(out, "#\tderived from %ws\n", fn1->name());
      first = false;
    }
    fn1->savenxihead(out, file, 0, 0);
  }

  if(first)
  {
    fprintf(out, "#\tderived from %ws\n", name());
    fprintf(out, "#\tno usabled files found\n", name());
  }

  fclose(out);

  return 0;
}

