#include "nxlib.h"
#include "nxnzb.h"
#include "crc32.h"



NXLFSFolderNZB::NXLFSFolderNZB(const wchar_t* path, const wchar_t* name, __time64_t time, NXLFSFolder* next)
: NXLFSFolder(name, time, next)
{
  m_path = NXwcsdup(path);
}



NXLFSFolderNZB::~NXLFSFolderNZB()
{
  NXfree(m_path); m_path = NULL;
}



/*virtual*/int NXLFSFolderNZB::build()
{
  if(m_build) return m_buildr;
  //NXMutexLocker ml(m_mutex, "build", "n/a");
  m_build = true;

  /*
  NXLFS* nxlfs = NXLFS::i();
  if(nxlfs)
  {
    m_buildf = new NXLFSFileBuild();
  }
  else
  */

  m_buildr = buildr();
  return m_buildr;
}



int NXLFSFolderNZB::buildr()
{
  cleanff();

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  if(!cfg) return R(-1, "no config");

  NXNNTPPool* pool = NXNNTPPool::i();
  if(!pool) return R(-2, "no message pool");


  CRC32 crc;
  crc.hash(m_path);
  struct _stat fs;
  if(_wstat(m_path, &fs)) return R(-1, "stat error");
  crc.hash(&fs.st_mtime, sizeof(fs.st_mtime));
  crc.hash(&fs.st_size, sizeof(fs.st_size));

  char pathnxi[MAX_PATH];
  char pathnxh[MAX_PATH];
  char patherr[MAX_PATH];
  if(cfg->cache())
  {
    if(sprintf_s(patherr, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "nzb", (crc.get() & 0xF0000000) >> 28, crc.get(), "err") == -1) return R(-11, "buffer error");
    if(_access(patherr, 0x04) == 0)
    {
      struct _stat fs;
      if(_stat(patherr, &fs)) return R(-12, "stat error");
      m_files = new NXLFSFileFS(patherr, fs.st_size, L"!errors.txt", fs.st_mtime, m_files);
    }

    if(sprintf_s(pathnxi, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "nzb", (crc.get() & 0xF0000000) >> 28, crc.get(), "nxi") == -1) return R(-13, "buffer error");
    if(sprintf_s(pathnxh, MAX_PATH, "%s\\%s\\%X\\%08X.%s", cfg->cache(), "nzb", (crc.get() & 0xF0000000) >> 28, crc.get(), "nxh") == -1) return R(-14, "buffer error");
    if(_access(pathnxh, 0x04) == 0 && _access(pathnxi, 0x04) == 0)
    {
      int r = NXLFSFileNews::loadnxh(pathnxh, pathnxi, m_files);
      if(r)
      {
        cleanff();
        _unlink(pathnxh);
        _unlink(pathnxi);
        char msg[MAX_PATH+50];
        sprintf_s(msg, MAX_PATH+50, "loadnxh error %d in %ws", r, pathnxh);
        m_files = new NXLFSFileText(msg, L"!message.txt", m_time, m_files);
      }
      if(r == 0)
        return buildc();
    }
  }


  NXNzb nzb;
  if(nzb.load(m_path))
  {
    m_files = new NXLFSFileText(nzb.err(), L"!message.txt", m_time, m_files);
    return -1;
  }


  //--- prevent stack overflow on monster nzbs ---
  bool usestack = true;
  auto_free<const char*> afheadmsgid;
  auto_free<int> afheadmsgidok;
  auto_free<const char*> afgetmsgid;
  auto_free<int> afgetmsgidok;
  auto_free<char> afgetmsgidd;
  if( cfg->nzbheadcheck() ? (sizeof(const char*)*nzb.segmentc + sizeof(int)*nzb.segmentc) : 0 +
      sizeof(int)*nzb.filec +
      sizeof(const char*)*nzb.filec +
      (sizeof(NXPart)+NXNEWSHEAD)*nzb.filec > 256*1024)
  {
    usestack = false;
  }


  //--- head the msgs ---
  int* headmsgidok = NULL;
  if(cfg->nzbheadcheck())
  {
    const char** headmsgid = usestack ? (const char**)NXalloca(sizeof(const char*)*nzb.segmentc)
                                      : afheadmsgid.get(nzb.segmentc);
    int segmentc = 0;
    for(int file = 0; file < nzb.filec; file++)
    {
      for(int segment = 0; segment < nzb.files[file].segmentc; segment++, segmentc++)
      {
        headmsgid[segmentc] = nzb.files[file].segments[segment].msgid;
      }
    }
    headmsgidok = usestack ? (int*)NXalloca(sizeof(int)*segmentc)
                           : afheadmsgidok.get(segmentc);
    clock_t start = clock();
    //if(pool->bhead(headmsgidok, headmsgid, segmentc) < 0)
    if(pool->bmhead(headmsgidok, headmsgid, segmentc) < 0)
    {
      m_files = new NXLFSFileText("error getting key file segment data", L"!message.txt", m_time, m_files);
      return R(-21, "head msgid error");
    }
    clock_t end = clock();
    if(NXLOG.perf)
      printf("nzbhead:\t%d headers in %.2fsec (%.2fh/s)\n", segmentc, (double)(end-start)/CLOCKS_PER_SEC, ((double)segmentc / ((end-start)/CLOCKS_PER_SEC)));
    /*
    for(int i = 0; i < segmentc; i++)
    {
      if(headmsgidok[i])
      {
        m_files = new NXLFSFileText("missing key file segment data", L"!message.txt", m_time, m_files);
        return R(-22, "head msgid error");
      }
    }
    */

    if(cfg->cache())
    {
      segmentc = 0;
      FILE* fh = NULL;
      for(int file = 0; file < nzb.filec; file++)
      {
        bool tf = false;
        for(int segment = 0; segment < nzb.files[file].segmentc; segment++, segmentc++)
        {
          if(headmsgidok[segmentc] == 0) continue;
          if(!fh)
          {
            if(fopen_s(&fh, patherr, "w")) return R(-23, "open error");
            if(!fh) return R(-24, "open error");
            fprintf(fh, "The following files have missing segments:\n");
          }
          if(!tf)
          {
            fprintf(fh, "%s:\n", nzb.files[file].filename);
            tf = true;

            //--- note: the cast shows how dangerous it is, but there is room to do it ---
            if(m_flaginc)
            {
              char* fn = const_cast<char*>(nzb.files[file].filename);
              _strupr(fn);
              strcat(fn, "!");
            }
          }
          fprintf(fh, "  segment %d\t<%s>\n", segment+1, nzb.files[file].segments[segment].msgid);
        }
      }
      if(fh) 
      {
        fclose(fh);

        struct _stat fs;
        if(_stat(patherr, &fs)) return R(-25, "stat error");
        m_files = new NXLFSFileFS(patherr, fs.st_size, L"!errors.txt", fs.st_mtime, m_files);
      }
    }
  }


  //--- get msgids ---
  const char** getmsgid = usestack ? (const char**)NXalloca(sizeof(const char*)*nzb.filec)
                                   : afgetmsgid.get(nzb.filec);
  int segmentc = 0;
  for(int file = 0; file < nzb.filec; segmentc += nzb.files[file].segmentc, file++)
  {
    int any = -1;
    bool all = true;
    bool first = true;
    bool last = true;
    if(headmsgidok)
    {
      first = headmsgidok[segmentc] == 0;
      last = headmsgidok[segmentc+nzb.files[file].segmentc-1] == 0;
      for(int segment = 0; segment < nzb.files[file].segmentc; segment++)
      {
        if(headmsgidok[segmentc+segment])
        {
          all = false;
          break;
        }
        any = segment;
      }
    }

    wchar_t fn[MAX_PATH];
    if(swprintf_s(fn, MAX_PATH, L"%hs", nzb.files[file].filename) == -1) return R(-31, "buffer error");

    NXLFSFileRAR::Fni fni;
    NXLFSFileRAR::analyze(fni, fn);

    if(first && fni.israr) 
      getmsgid[file] = nzb.files[file].segments[0].msgid;
    else 
    if(last) 
      getmsgid[file] = nzb.files[file].segments[nzb.files[file].segmentc-1].msgid;
    else
    if(any >= 0)
      getmsgid[file] = nzb.files[file].segments[any].msgid;
    else
      getmsgid[file] = NULL;
  }


  //--- get the msgs we need for size ---
  int* getmsgidok = usestack ? (int*)NXalloca(sizeof(int)*nzb.filec)
                             : afgetmsgidok.get(nzb.filec);
  char* getmsgidd = usestack ? (char*)NXalloca((sizeof(NXPart)+NXNEWSHEAD)*nzb.filec)
                             : afgetmsgidd.get((sizeof(NXPart)+NXNEWSHEAD)*nzb.filec); // fake-type!
  if(pool->bget(getmsgidok, getmsgid, nzb.filec, getmsgidd, NXNEWSHEAD) < 0)
  {
    m_files = new NXLFSFileText("error getting key file segment data", L"!message.txt", m_time, m_files);
    return R(-32, "get msgid error");
  }
  for(int file = 0; file < nzb.filec; file++)
  {
    if(getmsgid[file] && getmsgidok[file])
    {
      m_files = new NXLFSFileText("missing key file segment data", L"!message.txt", m_time, m_files);
      return R(-33, "get msgid error");
    }
  }


  //--- create nzb files ---
  int segment = 0;
  for(int file = 0; file < nzb.filec; file++)
  {
    if(NXfextmchk(nzb.files[file].filename, cfg->nzbextfilter()) || NXfnammchk(nzb.files[file].filename, cfg->nzbnamefilter())) continue;
        
    NXPart* part = (NXPart*)(getmsgidd+file*(sizeof(NXPart)+NXNEWSHEAD));
    __int64 size = part->size;
    int blocksize = part->blocksize;

    if(size == 0 || blocksize == 0)
    {
      continue;
    }

    int segmentc = nzb.files[file].segmentc;
    NXLFSFileNews::Blockinfo* bi = (NXLFSFileNews::Blockinfo*)NXcalloc(segmentc, sizeof(NXLFSFileNews::Blockinfo));
    for(int b = 0; b < segmentc; b++, segment++)
    {
      bi[b].status = !headmsgidok || headmsgidok[segment] == 0 ? 0 : 1;
      bi[b].start = 0;
      bi[b].end =
      bi[b].size = blocksize;
      strcpy_s(bi[b].msgid, NXMAXMSGID, nzb.files[file].segments[b].msgid);
    }
    if(segmentc > 1)
    {
      bi[segmentc-1].end =
      bi[segmentc-1].size = size % blocksize;
    }

    m_files = new NXLFSFileNews(segmentc, bi, nzb.files[file].filename, nzb.files[file].time, m_files);
        
    if(getmsgid[file] == nzb.files[file].segments[0].msgid)
      m_files->sethead(&part->data);
  }

  if(cfg->cache())
  {
    savenxh(pathnxh);
    savenxi(pathnxi);
  }

  return buildc();
}



int NXLFSFolderNZB::savenxi(const char* path)
{
  FILE* out = NULL;
  if(fopen_s(&out, path, "wb")) return R(-1, "open error");
  if(out == NULL) return R(-2, "open error");

  fprintf(out, "#\tderived from %ws\n", m_path);

  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    const NXLFSFileNews* f = dynamic_cast<const NXLFSFileNews*>(file);
    if(!f) continue;
    f->savenxi(out);
  }

  fclose(out);

  return 0;
}



int NXLFSFolderNZB::savenxh(const char* path)
{
  FILE* out = NULL;
  if(fopen_s(&out, path, "wb")) return R(-1, "open error");
  if(out == NULL) return R(-2, "open error");

  fprintf(out, "#\tderived from %ws\n", m_path);

  for(const NXLFSFile* file = m_files; file; file = file->next())
  {
    const NXLFSFileNews* f = dynamic_cast<const NXLFSFileNews*>(file);
    if(!f) continue;
    f->savenxihead(out, f, 0, 0);
  }

  fclose(out);

  return 0;
}
