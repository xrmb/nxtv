#include "nxlib.h"



/*static*/ bool NXLFSFolder::m_collections = true;
/*static*/ bool NXLFSFolder::m_flaginc = true;



NXLFSFolder::NXLFSFolder(const wchar_t* name, __time64_t time, NXLFSFolder* next)
{
  m_build = false;
  m_buildr = -1;

  m_next = next;

  m_name = NXwcsdup(name);
  m_time = time;

  m_hfolders = NULL;

  m_folders = NULL;
  m_files = NULL;

  //m_mutex = new NXMutex("folder");
}



NXLFSFolder::NXLFSFolder(const char* name, __time32_t time, NXLFSFolder* next)
{
  m_build = false;
  m_buildr = -1;

  m_next = next;

  m_name = NXstrdupw(name);
  m_time = time;

  m_hfolders = NULL;

  m_folders = NULL;
  m_files = NULL;

  //m_mutex = new NXMutex("folder");
}



NXLFSFolder::~NXLFSFolder()
{
  //m_mutex->lock("~", "n/a");

  //--- really long lists can cause stack overflow ---
  //delete m_next; m_next = NULL;
  while(m_next)
  {
    NXLFSFolder* d = m_next;
    m_next = m_next->m_next;
    d->m_next = NULL;
    delete d;
  }

  NXfree(m_name); m_name = NULL;

  cleanff();
  //m_mutex->unlock();
  //delete m_mutex; m_mutex = NULL;
}



void NXLFSFolder::cleanff()
{
  delete m_folders; m_folders = NULL;
  delete m_files; m_files = NULL;

  delete m_hfolders; m_hfolders = NULL;
}



const NXLFSFolder* NXLFSFolder::folders() const
{ 
  const_cast<NXLFSFolder*>(this)->buildi(); 
  return m_folders; 
}



const NXLFSFile* NXLFSFolder::files() const
{ 
  const_cast<NXLFSFolder*>(this)->buildi(); 
  return m_files; 
}



int NXLFSFolder::buildi()
{
  if(m_build) return m_buildr;

  //NXMutexLocker ml(m_mutex, "buildi", "n/a");

  m_buildr = build();
  if(m_buildr && !findfile(L"!message.txt"))
  {
    char msg[100];
    sprintf_s(msg, 100, "build error %d", m_buildr);
    //fprintf(stderr, "build error %d\n", m_buildr);
    m_files = new NXLFSFileText(msg, L"!message.txt", m_time, m_files);
  }
  return m_buildr;
}



int NXLFSFolder::buildc()
{
  if(!m_collections) return 0;

  //NXMutexLocker ml(m_mutex, "buildc", "n/a");

  C* c = NULL;
  auto_deletev<C> adc(&c);

  NXLFSFileRAR::Fni fni;
  for(NXLFSFile* ff = m_files; ff; ff = ff->next())
  {
    if(NXLFSFileRAR::analyze(fni, ff->name()) < 0) continue;
    C* cc;
    for(cc = c; cc; cc = cc->n)
    {
      if(cc->l == fni.bnl && wcsncmp(cc->i, ff->name(), fni.bnl) == 0)
      {
        break;
      }
    }
    if(!cc) c = cc = new C(ff->name(), fni.bnl, c);
    if(cc->f[fni.part]) return R(-1, "naming conflict"); // could have test.rar and test.part01.rar in same dir
    cc->f[fni.part] = ff;
    cc->k |= fni.kaputt;
  }


  for(C* cc = c; cc; cc = cc->n)
  {
    bool ok = !cc->k;
    for(int i = 0; ok && i < NXRARP-1; i++)
    {
      if(cc->f[i] == NULL && cc->f[i+1])
      {
        ok = false;
      }
    }

    if(ok)
    {
      releasefiles(cc->f);
      m_folders = new NXLFSFolderRAR(cc->f[0], cc->i, cc->f[0]->time(), m_folders);
    }
  }

  return 0;
}



/*
  files are in m_files
  removes files from m_files
  fixes each file next to the list order
*/
void NXLFSFolder::releasefiles(NXLFSFile** files)
{
  //NXMutexLocker ml(m_mutex, "releasefiles", "n/a");

  for(NXLFSFile** f = files; *f; f++)
  {
    if(NXLFS::i()) NXLFS::i()->rmSize((*f)->size());
    if(m_files == *f)
    {
      m_files = m_files->next();
    }
    else
    {
      for(NXLFSFile* ff = m_files; ff; ff = ff->next())
      {
        if(ff->next() == *f)
        {
          ff->renext(ff->next()->next());
          break;
        }
      }
    }

    (*f)->renext(f[1]);
  }
}



const NXLFSFolder* NXLFSFolder::findfolder(const wchar_t* path) const
{
  if(path == NULL) return NULL; // ERR
  //NXMutexLocker ml(m_mutex, "findfolder", "n/a");

  //if(path[0] != L'/' && path[0] != L'\\') return NULL; // ERR
  while(path[0] == L'/' || path[0] == L'\\') path++; // ERR

  const wchar_t* s = wcschr(path, L'/');
  if(s == NULL) s = wcschr(path, L'\\');
  if(s == NULL) 
  {
    if(path[0] == L'\0') return this;
    s = path+wcslen(path);
  }

  const NXLFSFolder* fd = folders();
  while(fd)
  {
    if(_wcsnicmp(fd->name(), path, s-path) == 0)
    {
      if(*s)
        return fd->findfolder(s+1);
      return fd;
    }
    fd = fd->next();
  }

  return NULL;
}



const NXLFSFile* NXLFSFolder::findfile(const wchar_t* path) const
{
  if(path == NULL) return NULL; // ERR
  //NXMutexLocker ml(m_mutex, "findfile", "n/a");

  while(path[0] == L'.' && (path[1] == L'/' || path[1] == L'\\')) path+=2; // note: there for nxpar2
  while(path[0] == L'/' || path[0] == L'\\') path++;

  const NXLFSFolder* fd = folders();

  const wchar_t* s = wcschr(path, L'/');
  if(s == NULL) s = wcschr(path, L'\\');
  if(s)
  {
    while(fd)
    {
      if(_wcsnicmp(fd->name(), path, s-path) == 0)
      {
        return fd->findfile(s+1);
      }
      fd = fd->next();
    }

    return NULL;
  }

  const NXLFSFile* ff = files();
  while(ff)
  {
    if(_wcsicmp(ff->name(), path) == 0)
    {
      break;
    }
    ff = ff->next();
  }
  return ff;
}



void NXLFSFolder::unload()
{
  //NXMutexLocker ml(m_mutex, "unload", "n/a");
  cleanff();

  m_build = false;
  m_buildr = -1;
}



void NXLFSFolder::adopt(NXLFSFolder* folder)
{
  NXLFSFolder* fd = m_hfolders;
  while(fd && fd->m_next) fd = fd->m_next;
  if(fd)
    fd->m_next = folder;
  else
    m_hfolders = folder;
  
  fd = m_folders;
  while(fd && fd->m_next) fd = fd->m_next;
  if(fd)
    fd->m_next = folder->m_folders;
  else
    m_folders = folder->m_folders;
  folder->m_folders = NULL;

  NXLFSFile* ff = m_files;
  while(ff && ff->m_next) ff = ff->m_next;
  if(ff)
    ff->m_next = folder->m_files;
  else
    m_files = folder->m_files;
  folder->m_files = NULL;
}



NXLFSFolder::C::C(const wchar_t* name, size_t len, C* next) 
{ 
  wcscpy_s(i, MAX_PATH, name);
  i[len] = L'\0';
  n = next; 
  memset(f, 0, (NXRARP+1)*sizeof(NXLFSFile*)); 
  l = len;
  k = false;
}



NXLFSFolder::C::~C() 
{ 
  delete n; n = NULL; // todo: long list stack overflow (unlikely here)
}
