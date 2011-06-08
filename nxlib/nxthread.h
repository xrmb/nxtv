#pragma once
#include "misc.h"
#include "nxmutex.h"
#include <windows.h>

typedef DWORD (WINAPI NXThreadFct)(LPVOID lpThreadParameter);



template<typename t>
class NXThread
{
public:
  NXThread(NXThreadFct* entry, t* data)
  {
    m_data = data;
    m_terminate = false;
    m_susp = false;
    m_req = false;
    m_done = false;
    m_ec = 0;
    InitializeCriticalSection(&m_sema);
    m_th = CreateThread(NULL, 0, entry, this, 0, &m_tid);
  }



  ~NXThread()
  {
    terminate();

    WaitForSingleObject(m_th, INFINITE);
    CloseHandle(m_th);

    DeleteCriticalSection(&m_sema);
  }



  void suspend(long howlong = INFINITE) 
  { 
    if(m_terminate) return;

    m_susp = true; 
    long s = 0;
    for(;;)
    {
      if(m_susp == false) return;
      if(m_terminate) return;
      Sleep(13);
      s += 13;
      if(howlong != INFINITE && s >= howlong) return;
    }
  }

  bool suspended() const { return m_susp; }
  


  void resume() 
  { 
    m_susp = false; 
  }



  void waitforsuspend() const
  {
    for(;;)
    {
      if(!m_susp && !m_terminate)
        Sleep(13);
      else
        return;
    }
  }



  int request() 
  { 
    if(m_req)
      return R(-1, "thread already requested");
    
    m_req = true; 
    return 0;
  }
  bool requested() const { return m_req; }



  int release()
  {
    if(!m_req)
      return R(-1, "thread not requested");

    m_req = false; 
    return 0;
  }



  void terminate() { m_terminate = true; resume(); }
  bool terminated() const { return m_terminate; }

  int exit(int ec) { m_done = true; m_ec = ec; return m_ec; }
  bool done() const { return m_done; }

  t* data() const { return m_data; }

private:
  HANDLE m_th;
  unsigned long m_tid;
  bool m_terminate;
  CRITICAL_SECTION m_sema;
  t* m_data;
  bool m_susp;
  bool m_req;
  bool m_done;
  int m_ec;
};



template<typename t>
class NXThreadPool
{
public:
  NXThreadPool(int count, NXThreadFct* entry, t** data)
  {
    m_count = count;
    m_t = (NXThread<t>**)NXcalloc(m_count, sizeof(NXThread<t>*));
    for(int i = 0; i < m_count; i++)
    {
      m_t[i] = new NXThread<t>(entry, data ? data[i] : NULL);
    }
    m_mutex = new NXMutex("NXThreadPool");
  }


  NXThreadPool(int count, NXThreadFct* entry, t* data)
  {
    m_count = count;
    m_t = (NXThread<t>**)NXcalloc(m_count, sizeof(NXThread<t>*));
    for(int i = 0; i < m_count; i++)
    {
      m_t[i] = new NXThread<t>(entry, data ? data : NULL);
    }
    m_mutex = new NXMutex("NXThreadPool");
  }



  ~NXThreadPool()
  {
    for(int i = 0; i < m_count; i++)
    {
      m_t[i]->terminate();
      m_t[i]->waitforsuspend();
      delete m_t[i]; m_t[i] = NULL;
    }
    delete m_mutex; m_mutex = NULL;
    NXfree(m_t); m_t = NULL;
  }


/*
  int waitforsuspend(int i) const 
  { 
    if(i < 0 || i >= m_count) 
      return R(-1, "thread out of range");
    
    m_t[i]->waitforsuspend(); 
    return 0;
  }
*/


  int suspended(int i) const 
  { 
    if(i < 0 || i >= m_count) 
      return R(-1, "thread out of range");
    return m_t[i]->suspended(); 
  }



  NXThread<t>* idle() const 
  { 
    NXMutexLocker ml(m_mutex, "idle", "n/a");
    for(int i = 0; i < m_count; i++) 
    {
      if(!m_t[i]->requested() && m_t[i]->suspended()) 
      {
        return m_t[i]; 
      }
    }
    return NULL; 
  }
  


  NXThread<t>* thread(int i) const 
  { 
    if(i >= 0 && i < m_count) 
      return m_t[i]; 
    R(-1, "thread out of range");
    return NULL;
  }



  NXThread<t>* waitforidle(bool request, bool* requested = NULL) 
  { 
    for(;;) 
    { 
      m_mutex->lock("waitforidle", "n/a");
      bool all = true;
      for(int i = 0; i < m_count; i++) 
      {
        if(!m_t[i]->done())
        {
          all = false;
          break;
        }
      }
      if(all)
      {
        return NULL;
      }

      for(int i = 0; i < m_count; i++) 
      {
        bool r = m_t[i]->requested();
        bool s = m_t[i]->suspended();
        if(!r && s || requested && requested[i] && s) 
        {
          if(request && (!requested || !requested[i]))
            m_t[i]->request();
          m_mutex->unlock();
          return m_t[i]; 
        }
      }
      m_mutex->unlock();
      Sleep(29); 
    } 
  }



private:
  NXMutex* m_mutex;
  NXThread<t>** m_t;
  int m_count;
};
