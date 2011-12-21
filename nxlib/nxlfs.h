#pragma once

#include "nxlib.h"

class NXFile;

class NXLFSFile;
class NXLFSFolder
{
public:
  NXLFSFolder(const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  NXLFSFolder(const char* name, __time32_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolder();

  const NXLFSFolder* folders() const;
  const NXLFSFile* files() const;

  const NXLFSFolder* next() const { return m_next; }

  __time64_t time() const { return m_time; }
  const wchar_t* name() const { return m_name; }

  const NXLFSFolder* findfolder(const wchar_t* path) const;
  const NXLFSFile* findfile(const wchar_t* path) const;

  virtual int build() = 0;

  static void collections(bool s) { m_collections = s; }
  static void flaginc(bool s) { m_flaginc = s; }
  void unload();

protected:
  // todo: get rid of class, alloc on stack
  class C
  {
  public:
    C(const wchar_t* name, size_t len, C* next);
    ~C();

    wchar_t i[MAX_PATH];
    NXLFSFile* f[NXRARP+1];   // +1 because we want the last one to always be NULL
    C* n;
    size_t l;
    bool k;
  };

  void cleanff();
  int buildc();
  int buildi();
  void releasefiles(NXLFSFile** files);
  void adopt(NXLFSFolder* folder);

  bool m_build;
  int m_buildr;

  NXLFSFolder* m_hfolders;

  NXLFSFolder* m_folders;
  NXLFSFile* m_files;

  NXLFSFolder* m_next;

  __time64_t m_time;
  wchar_t* m_name;

  //NXMutex* m_mutex;
  static bool m_collections;
  static bool m_flaginc;
};



class NXLFSFolderFS : public NXLFSFolder
{
public:
  NXLFSFolderFS(const wchar_t* path, const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderFS();

  virtual int build();

private:
  int buildr();
  wchar_t* m_path;
};



class NXLFSFolderNZB : public NXLFSFolder
{
public:
  NXLFSFolderNZB(const wchar_t* path, const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderNZB();

  virtual int build();

  int savenxi(const char* path);
  int savenxh(const char* path);

private:
  int buildr();
  wchar_t* m_path;
};



class NXLFSFolderNXI : public NXLFSFolder
{
public:
  NXLFSFolderNXI(const char* path, const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderNXI();

  virtual int build();

private:
  int buildr();
  char* m_path;
};



class NXLFSFolderRemote : public NXLFSFolder
{
public:
  struct Settings
  {
    char r[MAX_PATH];
    char c[MAX_PATH];
  };

  NXLFSFolderRemote(Settings& settings, const char* path, const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  NXLFSFolderRemote(Settings& settings, const char* path, const char* name, __time32_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderRemote();

  virtual int build();

protected:
  int buildr();

private:
  char* m_path;
  Settings& m_settings;
};



class NXLFSFolderRemoteRoot : public NXLFSFolderRemote
{
public:
  NXLFSFolderRemoteRoot(const wchar_t* nxr, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderRemoteRoot();

  virtual int build();
  const Settings& settings() const { return m_settings; }

private:
  int buildr();
  wchar_t* m_nxr;
  Settings m_settings;
};



class NXLFSFolderRAR : public NXLFSFolder
{
public:
  NXLFSFolderRAR(NXLFSFile* file, const wchar_t* name, __time64_t time, NXLFSFolder* next = NULL);
  virtual ~NXLFSFolderRAR();

  virtual int build();

  int savenxh(const char* path);
  int savenxi(const char* path);
  int savenxi(FILE* out);

private:
  int buildr();
  int buildrr();
  NXLFSFile* m_file;
  char* m_err;
};



class NXLFSFile
{
friend class NXLFSFolder;
friend class NXLFSFolderRAR;
public:
  NXLFSFile(const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  NXLFSFile(const char* name, __time32_t time, NXLFSFile* next = NULL);
  virtual ~NXLFSFile();

  const NXLFSFile* next() const { return m_next; }

  virtual int open() = 0;
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL) = 0;
  virtual int close() = 0;

  virtual int aread(size_t rlen, __int64 offset) { return 0; }
  virtual int pread(void* buffer, size_t blen, size_t* rlen, __int64 offset) { return 0; }

  __int64 size() const { return m_size; }
  __time64_t time() const { return m_time; }
  const wchar_t* name() const { return m_name; }
  int rename(const wchar_t* name);

  void sethead(const char* data) { NXfree(m_head); m_head = (char*)NXmalloc(NXNEWSHEAD); memcpy(m_head, data, NXNEWSHEAD); }

protected:
  void renext(NXLFSFile* next) { m_next = next; }
  NXLFSFile* next() { return m_next; }

  __int64 m_size;
  char* m_head;

private:
  __time64_t m_time;

  wchar_t* m_name;
  NXLFSFile* m_next;
};



class NXLFSFileFS : public NXLFSFile
{
public:
  NXLFSFileFS(const wchar_t* path, __int64 size, const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  NXLFSFileFS(const char* path, __int64 size, const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  virtual ~NXLFSFileFS();

  virtual int open();
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL);
  virtual int close();

private:
  char* m_path;
  FILE* m_fh;
};



class NXLFSFileText : public NXLFSFile
{
public:
  NXLFSFileText(const char* text, const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  NXLFSFileText(const char* text, const char* name, __time32_t time, NXLFSFile* next = NULL);
  virtual ~NXLFSFileText();

  virtual int open();
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL);
  virtual int close();

private:
  char* m_text;
};



class NXLFSFileBuild : public NXLFSFile
{
public:
  NXLFSFileBuild(unsigned int pos, unsigned int steps, NXLFSFile* next = NULL);
  virtual ~NXLFSFileBuild();

  virtual int open();
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL);
  virtual int close();

  int setStep(unsigned int step, unsigned int substeps);
  int setSubstep(unsigned int substep);

private:
  unsigned int m_pos;
  unsigned int m_step;
  unsigned int m_steps;
  unsigned int m_substeps;
};



class NXLFSFileRAR : public NXLFSFile
{
public:
  class Piece
  {
  public:
    Piece(NXLFSFile* f, __int64 pos, size_t len, Piece*& c) : f(f), pos(pos), len(len), n(NULL) { if(c == NULL) { c = this; } else { Piece* cc = c; while(cc->n) cc = cc->n; cc->n = this; } }
    ~Piece() { delete n; n = NULL; } // todo: long list stack overflow (unlikely here)

    __int64 size() const { __int64 r = 0; for(const Piece* p = this; p; p = p->n) r += p->len; return r; }

    NXLFSFile* f; // rar folder owns it
    __int64 pos;
    size_t len;
    Piece* n;
  };

  //NXLFSFileRAR(Piece* pieces, const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  NXLFSFileRAR(Piece* pieces, const char* name, __time32_t time, NXLFSFile* next = NULL);
  virtual ~NXLFSFileRAR();

  virtual int open();
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL);
  virtual int close();

  const Piece* pieces() const { return m_pieces; }

  struct Fni
  {
    bool israr;
    bool kaputt;  // file marked as broken by !
    size_t bnl;   // base name length
    int part;
    int ns;       // naming schema
  };
  static int analyze(Fni& fni, const wchar_t* file);

private:
  Piece* m_pieces;
  NXLFSFile* m_lrf; // last read file
};



class NXLFSFileNews : public NXLFSFile
{
public:
#pragma pack(push)
#pragma pack(1)
  struct Blockinfo
  {
    __int8 status;  // 0...confirmed there, 1...head missing, 2...get missing, 3...not in nzb
    size_t size;
    size_t start;
    size_t end;
    char msgid[NXMAXMSGID];
  };
#pragma pack(pop)

  NXLFSFileNews(size_t blocks, Blockinfo* bi, const wchar_t* name, __time64_t time, NXLFSFile* next = NULL);
  NXLFSFileNews(size_t blocks, Blockinfo* bi, const char* name, __time32_t time, NXLFSFile* next = NULL);
  virtual ~NXLFSFileNews();

  virtual int open();
  virtual int read(void* buffer, size_t blen, size_t* rlen, __int64 offset, int readahead, NXLFSFile* rahint = NULL);
  virtual int close();

  virtual int aread(size_t rlen, __int64 offset);
  virtual int pread(void* buffer, size_t blen, size_t* rlen, __int64 offset);

  size_t blocks() const { return m_blocks; }
  size_t blocksize(size_t block) const { if(block >= 0 && block < m_blocks) return m_bi[block].size; return R(-1, "block out of range"); return 0; }
  size_t blocksizemax() const { size_t r = 0; for(size_t b = 0; b < m_blocks; b++) r = max(r, m_bi[b].size); return r; }
  int blockinfo(Blockinfo*& bi, size_t rlen, __int64 offset) const;
  const Blockinfo* blockinfo() const { return m_bi; }
  const Blockinfo* blockinfo(size_t block) const { if(block >= 0 && block < m_blocks) return &m_bi[block]; R(-1, "block out of range"); return NULL; }

  static int loadnxi(const char* pathnxi, NXLFSFile*& next);
  static int loadnxh(const char* pathnxh, const char* pathnxi, NXLFSFile*& next);
  int savenxi(FILE* out) const;
  void savenxibi(FILE* out, const Blockinfo* bi, size_t blocks, size_t blocksize) const;
  void savenxihead(FILE* out, const NXLFSFile* file, size_t blocks, size_t blocksize) const;
  size_t commonbs(const Blockinfo* bi, size_t blocks) const;

protected:
  NXLFSFileNews(const char* name, __int64 size, __time32_t time, NXLFSFile* next = NULL);

  virtual int loadbi() { return 0; }
  int parsebi(NXFile& nxi);

  Blockinfo* m_bi;
  size_t m_blocks;
};



class NXLFSFileNewsNXI : public NXLFSFileNews
{
public:
  //NXLFSFileNewsNXI(const wchar_t* nxi, const char* name, __int64 size, __time32_t time, NXLFSFile* next = NULL);
  NXLFSFileNewsNXI(const char* nxi, const char* name, __int64 size, __time32_t time, NXLFSFile* next = NULL);
  ~NXLFSFileNewsNXI();

protected:
  virtual int loadbi();

  char* m_nxi;
};



class NXLFSFileNewsHTTP : public NXLFSFileNews
{
public:
  NXLFSFileNewsHTTP(const char* nxi, const char* name, __int64 size, __time32_t time, NXLFSFile* next = NULL);
  ~NXLFSFileNewsHTTP();

protected:
  virtual int loadbi();

  char* m_nxi;
};



class NXLFSFileNewsNNTP : public NXLFSFileNews
{
public:
  NXLFSFileNewsNNTP(const char* msgid, const char* name, __int64 size, __time32_t time, NXLFSFile* next = NULL);
  ~NXLFSFileNewsNNTP();

protected:
  virtual int loadbi();

  char* m_msgid;
};



class NXLFS
{
public:
  NXLFS();
  ~NXLFS();

  int init(const wchar_t* home);

  void addSize(__int64 size);
  void rmSize(__int64 size);
  __int64 size() const { return m_size; }

  const NXLFSFolder* root() const { return m_root; }
  static NXLFS* i() { return m_i; }
/*
  NXLFSFolder* nextBuild();
  unsigned int queueBuild(NXLFSFolder* folder);
  unsigned int queuePos() const { return m_buildp; }
*/

private:
  static NXLFS* m_i;
  wchar_t* m_home;
  NXLFSFolder* m_root;
  NXMutex* m_mutex;

/*
  NXThread<NXLFS>* m_builder;
  NXLFSFolder** m_buildq;
  size_t m_builds;
  NXMutex* m_buildm;
  unsigned int m_buildp;
  unsigned int m_buildc;
*/

  __int64 m_size;
};
