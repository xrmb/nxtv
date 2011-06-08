#pragma once

#include <windows.h>


class NXMutex
{
public:
  NXMutex(const char* name);
  ~NXMutex();

  int lock(const char* locker1, const char* locker2);
  int unlock();

private:
  char* m_name;
  const char* m_locker1;
  const char* m_locker2;
  CRITICAL_SECTION m_h;
};



class NXMutexLocker
{
public:
  NXMutexLocker(NXMutex* m, const char* locker1, const char* locker2);
  ~NXMutexLocker();

  void lock();
  void unlock();

private:
  NXMutex* m_m;
  bool m_l;
  const char* m_l1;
  const char* m_l2;
};

