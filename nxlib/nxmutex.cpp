#include "nxlib.h"
#include "nxmutex.h"
#include "misc.h"



NXMutex::NXMutex(const char* name)
{
  m_name = NXstrdup(name);
  m_locker1 = NULL;
  m_locker2 = NULL;
  //m_count = 0;
  //m_h = CreateMutex(NULL, false, m_name);
  InitializeCriticalSection(&m_h);
}



NXMutex::~NXMutex()
{
  NXfree(m_name); m_name = NULL;
  DeleteCriticalSection(&m_h);
}



int NXMutex::lock(const char* locker1, const char* locker2)
{
  if(locker1 == NULL)
    return R(-1, "need to know lock requestor for mutex %s", m_name);

  if(NXLOG.mutex) 
    printf("mutex:\t%s %d lock request %s/%s\n", m_name, GetCurrentThreadId(), locker1, locker2);

  EnterCriticalSection(&m_h);
  if(NXLOG.mutex) 
    printf("mutex:\t%s %d lock granted %s/%s\n", m_name, GetCurrentThreadId(), locker1, locker2);

  if(m_h.RecursionCount == 1)
  {
    m_locker1 = locker1;
    m_locker2 = locker2;
  }

  return 0;
}



int NXMutex::unlock()
{
  if(NXLOG.mutex) 
    printf("mutex:\t%s %d unlock %s/%s\n", m_name, GetCurrentThreadId(), m_locker1, m_locker2);

  if(m_locker1 == NULL)
  {
    return R(-1, "unlocking unlocked mutex %s", m_name);
  }
  if(m_h.RecursionCount == 1)
  {
    m_locker1 = NULL;
    m_locker2 = NULL;
  }

  LeaveCriticalSection(&m_h);
  return 0;
}



NXMutexLocker::NXMutexLocker(NXMutex* m, const char* locker1, const char* locker2) 
: m_m(m),
  m_l1(locker1),
  m_l2(locker2),
  m_l(true)
{ 
  m_m->lock(locker1, locker2); 
}



NXMutexLocker::~NXMutexLocker() 
{ 
  if(m_l)
    m_m->unlock(); 
}



void NXMutexLocker::lock()
{
  if(!m_l)
    m_m->lock(m_l1, m_l2); 
  m_l = true;
}



void NXMutexLocker::unlock()
{
  if(m_l)
    m_m->unlock(); 
  m_l = false;
}

