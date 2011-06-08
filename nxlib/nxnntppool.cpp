#include "nxlib.h"

#include "nxthread.h"


/*static*/NXNNTPPool* NXNNTPPool::m_i = NULL;


unsigned long __stdcall NXNNTPPooler(void* nxthread)
{
  NXThread<NXNNTP>* t = reinterpret_cast<NXThread<NXNNTP>*>(nxthread);
  if(!t) { return 1; }

  NXNNTP* nxnntp = t->data();
  if(!nxnntp) { return t->exit(1); }

  int cid = nxnntp->cid();
  for(;;)
  {
    if(NXLOG.thread)
      printf("thread:\t%02d thread suspended\n", cid);
    t->suspend();
    if(NXLOG.thread)
      printf("thread:\t%02d thread running\n", cid);

    if(t->terminated())
    {
      if(NXLOG.thread)
        printf("thread:\t%02d thread terminating\n", cid);
      break;
    }

    int r;
    switch(nxnntp->busy())
    {
    case 1:
      r = nxnntp->tget();
      break;

    case 2:
      r = nxnntp->thead();
      break;

    case 3:
      r = nxnntp->tmhead();
      break;

    default:
      R(-1, "thread %02d running but nothing to-do", cid);
      break;
    }
  }

  return t->exit(0);
}



unsigned long __stdcall NXNNTPPoolIdleDisconnect(void* nxthread)
{
  NXThread<NXNNTPPool>* t = reinterpret_cast<NXThread<NXNNTPPool>*>(nxthread);
  if(!t) { return 1; }

  NXNNTPPool* pool = t->data();
  if(!pool) { return t->exit(1); }

  for(;;)
  {
    t->suspend(10000);

    if(t->terminated())
      break;

    pool->idledisconnect();
  }

  return t->exit(0);
}



NXNNTPPool::NXNNTPPool()
{
  m_count = 0;
  m_nxnntp = NULL;
  m_tp = NULL;
  m_tid = NULL;
  m_mutex = new NXMutex("NXNNTPPool");

  if(m_i) R(-1, "multiple instances of %s", "NXNNTPPool");
  m_i = this;
}



int NXNNTPPool::init(int count)
{
  WSADATA m_wsad;
  int r = WSAStartup(MAKEWORD(2, 2), &m_wsad);
  if(r != NO_ERROR) return R(-1, "WSAStartup error %d", r);

  if(count < 1) count = 1;
  if(count > NXMAXCONN) count = NXMAXCONN;
  m_count = count;

  m_nxnntp = (NXNNTP**)NXcalloc(m_count, sizeof(NXNNTP*));

  for(int i = 0; i < m_count; i++)
  {
    m_nxnntp[i] = new NXNNTP(i);
  }

  m_tp = new NXThreadPool<NXNNTP>(m_count, NXNNTPPooler, m_nxnntp);
  m_tid = new NXThread<NXNNTPPool>(NXNNTPPoolIdleDisconnect, this);

  return 0;
}



NXNNTPPool::~NXNNTPPool()
{
  if(m_i == this) m_i = NULL;

  delete m_tp; m_tp = NULL;
  delete m_tid; m_tid = NULL;
  delete m_mutex; m_mutex = NULL;

  for(int i = 0; i < m_count; i++)
  {
    delete m_nxnntp[i]; m_nxnntp[i] = NULL;
  }

  NXfree(m_nxnntp); m_nxnntp = NULL;

  WSACleanup();
}



void NXNNTPPool::shutdown()
{
  for(int i = 0; i < m_count; i++)
  {
    m_nxnntp[i]->giveup();
  }
}



int NXNNTPPool::get(const char* msgid)
{
  NXMutexLocker ml(m_mutex, "aget", msgid);

  NXThread<NXNNTP>* t = NULL;

  for(int i = 0; i < m_count; i++)
  {
    if(m_nxnntp[i]->busy(msgid) == 1)
    {
      if(NXLOG.thread)
        printf("thread:\t%02d thread running, waiting for aget %s\n", i, msgid);
      t = m_tp->thread(i);
      if(!t) return R(-1, "no such thread");
      t->request();
      t->waitforsuspend();
      int r = m_nxnntp[i]->agret();
      t->release();
      return r;
    }
  }

  t = m_tp->waitforidle(true);
  if(!t) return R(-1, "no idle thread");
  NXNNTP* n = t->data();
  n->aget(msgid);
  if(NXLOG.thread)
    printf("thread:\t%02d thread resume in %s\n", n->cid(), "get");
  if(NXLOG.get)
    printf("pool:\t%02d accepted %s %s\n", n->cid(), "get", msgid);
  t->resume();

  ml.unlock();

  t->waitforsuspend();
  int r = n->agret();
  t->release();

  return r;
}


/*
return:
  >0x10000  ... connection now working on it
  >0x20000  ... connection already working on it
  =0  ... all connections busy
*/
int NXNNTPPool::aget(const char* msgid)
{
  NXMutexLocker ml(m_mutex, "aget", msgid);

  for(int i = 0; i < m_count; i++)
  {
    if(NXLOG.thread)
      printf("thread:\t%02d suspended %d, busy %d\n", i, m_tp->suspended(i), m_nxnntp[i]->busy(msgid));
    if(m_nxnntp[i]->busy(msgid) == 1)
    {
      if(NXLOG.get > 1)
        printf("pool:\t%02d working on %s\n", i, msgid);
      return 0x10000 + i;
    }
  }

  NXThread<NXNNTP>* t = m_tp->idle();
  if(t)
  {
    NXNNTP* n = t->data();
    n->aget(msgid);
    if(NXLOG.thread)
      printf("thread:\t%02d thread resume in %s\n", n->cid(), "aget");
    if(NXLOG.get)
      printf("pool:\t%02d accepted %s %s\n", n->cid(), "aget", msgid);
    t->resume();
    return 0x20000 + n->cid();
  }

  return 0;
}



int NXNNTPPool::bhead(int* ok, const char** msgids, int count, NXCbBatchRequest* cb)
{
  return breq(false, ok, msgids, count, NULL, 0, cb);
}



int NXNNTPPool::bget(int* ok, const char** msgids, int count, char* data, size_t datalen, NXCbBatchRequest* cb)
{
  return breq(true, ok, msgids, count, data, datalen, cb);
}


  
int NXNNTPPool::breq(bool gorh, int* ok, const char** msgids, int count, char* data, size_t datalen, NXCbBatchRequest* cb)
{
  if(count == 0) return 0;

  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-1, "no cache");

  int* assign = (int*)_alloca(sizeof(int)*count);
  memset(assign, -1, sizeof(int)*count);

  bool* tused = (bool*)_alloca(sizeof(bool)*m_count);
  memset(tused, 0, sizeof(bool)*m_count);

  memset(ok, -1, sizeof(int)*count);
  if(data) memset(data, 0, (sizeof(NXPart)+datalen)*count);

  NXThread<NXNNTP>* t = NULL;
  int r = 0;

  //--- get/head all msgids ---
  for(int pos = 0; pos < count; pos++)
  {
    if(msgids[pos] == NULL)
      continue;

    t = m_tp->waitforidle(true, tused);
    if(!t) return R(-2, "no idle thread");
    NXNNTP* n = t->data();
    int cid = n->cid();

    //--- check if this thread did a head check for us ---
    if(tused[cid])
    {
      for(int nr = 0; nr < pos; nr++)
      {
        if(assign[nr] == cid)
        {
          ok[nr] = gorh ? n->agret() : n->ahret();
          t->release();
          tused[cid] = false;

          assign[nr] = -1;
          if(ok[nr] == 0 && data)
          {
            if(cache->find(msgids[nr], (NXPart*)(data+nr*(sizeof(NXPart)+datalen)), datalen) < 0)
              ok[nr] = -99;
          }
          if(cb) cb(nr);
          break;
        }
      }
      pos--;
      continue;
    }


    //--- assign get/head request to thread ---
    if(gorh)
    {
      if(n->aget(msgids[pos])) 
      {
        r = R(-3, "aget error");
        t->release();
        break;
      }
      if(NXLOG.get)
        printf("pool:\t%02d accepted %s %s\n", cid, "get", msgids[pos]);
    }
    else
    {
      if(n->ahead(msgids[pos])) 
      {
        r = R(-4, "ahead error");
        t->release();
        break;
      }
      
      if(NXLOG.head)
        printf("pool:\t%02d accepted %s %s\n", cid, "head", msgids[pos]);
    }

    assign[pos] = cid;
    tused[cid] = true;
    
    if(NXLOG.thread)
      printf("thread:\t%02d thread resume in %s\n", n->cid(), gorh ? "bget" : "bhead");
    t->resume();
  }

  //--- wait for all requests to finish ---
  for(int nr = 0; nr < count; nr++)
  {
    int i = assign[nr];
    if(i >= 0 && i < m_count)
    {
      t = m_tp->thread(i);
      t->waitforsuspend();
      ok[nr] = gorh ? t->data()->agret() : t->data()->ahret();
      t->release();

      if(ok[nr] == 0 && data)
      {
        if(cache->find(msgids[nr], (NXPart*)(data+nr*(sizeof(NXPart)+datalen)), datalen) < 0)
          ok[nr] = -99;
      }
      if(cb) cb(nr);
    }
  }

  if(r) return r;

  for(int nr = 0; nr < count; nr++)
  {
    if(ok[nr] && msgids[nr]) 
      r++;
  }

  return r;
}



int NXNNTPPool::bmhead(int* ok, const char** msgids, int count, NXCbBatchRequest* cb)
{
  if(count == 0) return 0;

  int* assign = (int*)_alloca(sizeof(int)*count);
  memset(assign, -1, sizeof(int)*count);

  bool* tused = (bool*)_alloca(sizeof(bool)*m_count);
  memset(tused, 0, sizeof(bool)*m_count);

  memset(ok, -1, sizeof(int)*count);

  NXThread<NXNNTP>* t = NULL;
  int r = 0;

  //--- get/head all msgids ---
  for(int pos = 0; pos < count; pos += NXMHEAD)
  {
    t = m_tp->waitforidle(true, tused);
    if(!t) return R(-1, "no idle thread");
    NXNNTP* n = t->data();
    int cid = n->cid();

    //--- check if this thread did a head check for us ---
    if(tused[cid])
    {
      for(int nr = 0; nr < pos; nr += NXMHEAD)
      {
        if(assign[nr] == cid)
        {
          int c = nr+NXMHEAD > count ? count % NXMHEAD : NXMHEAD;
          n->amhret(ok+nr, c);
          t->release();
          tused[cid] = false;

          for(int j = 0; j < c; j++)
          {
            assign[nr+j] = -1;
          }
          if(cb) cb(nr);
          break;
        }
      }
      pos -= NXMHEAD;
      continue;
    }

    //--- assign get/head request to thread ---
    int c = pos+NXMHEAD > count ? count % NXMHEAD : NXMHEAD;
    if(n->amhead(msgids+pos, c)) 
    {
      //--- todo: add random error to amhead and see if that works ---
      r = R(-2, "amhead error");
      t->release();
      break;
    }
    if(NXLOG.head)
      printf("pool:\t%02d accepted %s %d..%d\n", cid, "mhead", pos, pos+c);
    
    assign[pos] = cid;
    tused[cid] = true;

    if(NXLOG.thread)
      printf("thread:\t%02d thread resume in %s\n", n->cid(), "bmhead");
    t->resume();
  }

  //--- wait for all requests to finish ---
  for(int nr = 0; nr < count; nr += NXMHEAD)
  {
    int i = assign[nr];
    if(i >= 0 && i < m_count)
    {
      t = m_tp->thread(i);
      t->waitforsuspend();
      int c = nr+NXMHEAD > count ? count % NXMHEAD : NXMHEAD;
      t->data()->amhret(ok+nr, c);
      t->release();
    }
  }

  if(r) return r;
  for(int nr = 0; nr < count; nr++)
  {
    if(ok[nr]) 
      r++;
  }

  return r;
}



void NXNNTPPool::idledisconnect()
{
  NXMutexLocker ml(m_mutex, "idledisconnect", "n/a");

  for(int i = 0; i < m_count; i++)
  {
    m_nxnntp[i]->idledisconnect();
  }
}



int NXNNTPPool::idlecount() const
{
  NXMutexLocker ml(m_mutex, "idlecount", "n/a");

  int ic = 0;
  for(int i = 0; i < m_count; i++)
  {
    if(!m_nxnntp[i]->busy())
    {
      ic++;
    }
  }

  return ic;
}

