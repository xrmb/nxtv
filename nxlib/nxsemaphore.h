#pragma once

#include <windows.h>


class NXSemaphore
{
public:
  NXSemaphore(long init = 0, long max = LONG_MAX);
  ~NXSemaphore();

  void inc(long count = 1) const;
  void dec(long count = 1) const;
  void wait(long howlong = INFINITE) const;

private:
  HANDLE m_h;
};