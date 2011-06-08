#include "nxsemaphore.h"

#include <stdio.h>



NXSemaphore::NXSemaphore(long init, long max)
{
  m_h = CreateSemaphore(NULL, init, max, NULL);
}



NXSemaphore::~NXSemaphore()
{
  CloseHandle(m_h);
}


/*
long NXSemaphore::count() const
{
  long count;
  WaitForSingleObject(m_h, 0);
  ReleaseSemaphore(m_h, 1, &count);
  printf("sema count %d\n", count);
  return count;
}
*/


void NXSemaphore::inc(long count) const
{
  ReleaseSemaphore(m_h, count, NULL);
}



void NXSemaphore::dec(long count) const
{
  for(long i = 0; i < count; i++)
    WaitForSingleObject(m_h, 0);
}



void NXSemaphore::wait(long howlong) const
{
  WaitForSingleObject(m_h, howlong);
}
