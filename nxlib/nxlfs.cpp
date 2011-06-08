#include "nxlib.h"
#include "nxlfs.h"
#include "nxthread.h"



/*static*/ NXLFS* NXLFS::m_i = NULL;

/*
unsigned long __stdcall NXLFSBuilder(void* nxthread)
{
  NXThread<NXLFS>* t = reinterpret_cast<NXThread<NXLFS>*>(nxthread);
  if(!t) { return 1; }

  NXLFS* nxlfs = t->data();
  if(!nxlfs) { return t->exit(1); }

  for(;;)
  {
    t->suspend();

    if(t->terminated())
      break;

    NXLFSFolder* b = nxlfs->nextBuild();
    if(!b)
    {
      R(-1, "nothing to build");
      continue;
    }

    b->build();
  }

  return t->exit(0);
}
*/


NXLFS::NXLFS()
: m_root(NULL), m_home(NULL)
{
  if(m_i) R(-1, "multiple instances of %s", "NXLFS");
  m_i = this;

  m_size = 0;
  m_mutex = new NXMutex("NXLFS");

/*
  m_builds = 5;
  m_buildq = (NXLFSFolder**)NXcalloc(m_builds, sizeof(NXLFSFolder*));
  m_buildm = new NXMutex("NXLFSBuilder");
  m_buildc = 0;
  m_buildp = 0;
  m_builder = new NXThread<NXLFS>(NXLFSBuilder, this);
*/
}



NXLFS::~NXLFS()
{
  if(m_i == this) m_i = NULL;

  delete m_mutex; m_mutex = NULL;

/*  
  delete m_builder; m_builder = NULL;
  NXfree(m_buildq); m_buildq = NULL;
  delete m_buildm; m_buildm = NULL;
*/

  delete m_root; m_root = NULL;
  NXfree(m_home); m_home = NULL;
}



int NXLFS::init(const wchar_t* home)
{
  m_home = NXwcsdup(home);
  m_root = new NXLFSFolderFS(m_home, L"root", 0, NULL);
  return 0;
}


/*
NXLFSFolder* NXLFS::nextBuild() 
{
  NXMutexLocker ml(m_buildm, "nextBuild", "n/a");
  NXLFSFolder* r = m_buildq[0];
  memmove(m_buildq+1, m_buildq, sizeof(NXLFSFolder*)*(m_builds-1));
  m_buildq[m_builds-1] = NULL;
  m_buildp++;
  return r;
}



unsigned int NXLFS::queueBuild(NXLFSFolder* folder)
{
  NXMutexLocker ml(m_buildm, "nextBuild", "n/a");
  m_buildc++;
  for(size_t i = 0; i < m_builds; i++)
  {
    if(m_buildq[i] == NULL)
    {
      m_buildq[i] = folder;
      return m_buildc;
    }
  }

  m_buildq = (NXLFSFolder**)NXrealloc(m_buildq, m_builds*2);
  m_buildq[m_builds] = folder;
  for(size_t i = m_builds+1; i < m_builds*2; i++)
  {
    m_buildq[i] = NULL;
  }
  m_builds *= 2;
  return m_buildc;
}
*/


void NXLFS::addSize(__int64 size)
{
  NXMutexLocker ml(m_mutex, "addSize", "n/a");
  m_size += size;
}



void NXLFS::rmSize(__int64 size)
{
  NXMutexLocker ml(m_mutex, "rmSize", "n/a");
  m_size -= size;
}
