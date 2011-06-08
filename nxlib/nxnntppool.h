#pragma once

#include "nxthread.h"

typedef void (NXCbBatchRequest)(int index);


class NXNNTPPool
{
public:
  NXNNTPPool();
  ~NXNNTPPool();

  int init(int count);

  static NXNNTPPool* i() { return m_i; }
  int get(const char* msgid);
  int aget(const char* msgid);

  int bget(int* ok, const char** msgids, int count, char* data = NULL, size_t datalen = 0, NXCbBatchRequest* cb = NULL);
  int bhead(int* ok, const char** msgids, int count, NXCbBatchRequest* cb = NULL);
  int bmhead(int* ok, const char** msgids, int count, NXCbBatchRequest* cb = NULL);

  void idledisconnect();
  int idlecount() const;
  void shutdown();

private:
  int breq(bool gorh, int* ok, const char** msgids, int count, char* data, size_t datalen, NXCbBatchRequest* cb);

  int m_count;
  NXNNTP** m_nxnntp;
  NXThreadPool<NXNNTP>* m_tp;
  NXThread<NXNNTPPool>* m_tid;
  NXMutex* m_mutex;

  static NXNNTPPool* m_i;
};
