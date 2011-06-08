#pragma once
#include "misc.h"



template <typename X>
class auto_free
{
public:
  auto_free(size_t size, bool c = false) { m_ptr = (X*)(c ? NXcalloc(size, sizeof(X)) : NXmalloc(sizeof(X)*size)); if(m_ptr) m_size = size; else m_size = 0;}
  auto_free(X* ptr = NULL) { m_ptr = ptr; m_size = 0; }
  ~auto_free() { NXfree(m_ptr); m_ptr = NULL; m_size = 0; }
  
  X* get(size_t size, bool c = false) 
  { 
    if(size < m_size) 
    { 
      if(c) memset(m_ptr, 0, sizeof(X)*m_size); 
      return m_ptr; 
    }
    NXfree(m_ptr); 
    m_ptr = (X*)(c ? NXcalloc(size, sizeof(X)) : NXmalloc(sizeof(X)*size)); 
    if(m_ptr)
      m_size = size;
    else
      m_size = 0;
    return m_ptr; 
  }

  X* set(X* ptr) { m_size = 0; NXfree(m_ptr); m_ptr = ptr; return m_ptr; }
  X* release() { m_size = 0; X* r = m_ptr; m_ptr = NULL; return r; }
  X* resize(size_t size, bool c = false) 
  { 
    m_ptr = (X*)NXrealloc(m_ptr, sizeof(X)*size); 
    if(m_ptr) 
    {
      if(c && size > m_size) memset(m_ptr+sizeof(X)*m_size, 0, sizeof(X)*(size-m_size));
      m_size = size; 
    }
    else 
      m_size = 0; 
    return m_ptr; 
  }
  size_t size() const { return m_size; }
  operator X* () const { return m_ptr; }

private:
  X* m_ptr;
  size_t m_size;
};


template <typename X>
class auto_freea
{
public:
  auto_freea(size_t len) { m_ptr = NULL; m_len = 0; get(len); }
  auto_freea() { m_ptr = NULL; m_len = 0; }
  ~auto_freea() { clean(); }
  X* get(size_t len) { clean(); m_len = len, m_ptr = (X*)NXcalloc(m_len, sizeof(X)); return m_ptr; }
  void releaseouter() { NXfree(m_ptr); m_ptr = NULL; }
  X* release() { X* r = m_ptr; m_ptr = NULL; return r; }
  operator X* () const { return m_ptr; }

private:
  void clean()
  {
    if(m_ptr)
    {
      for(size_t i = 0; i < m_len; i++)
      {
        NXfree((void*)m_ptr[i]); m_ptr[i] = NULL;
      }
      NXfree(m_ptr); m_ptr = NULL;
    }
    m_len = 0;
  }

  size_t m_len;
  X* m_ptr;
};



class NXLFSFile;
class auto_close
{
public:
  auto_close()              { m_fh = NULL;  m_fd = 0;   m_ff = NULL; }
  auto_close(FILE* fh)      { m_fh = fh;    m_fd = 0;   m_ff = NULL; }
  auto_close(int fd)        { m_fh = NULL;  m_fd = fd;  m_ff = NULL; }
  auto_close(NXLFSFile* ff) { m_fh = NULL;  m_fd = 0;   m_ff = ff; }
  ~auto_close() { close(); }
  void close();
  void set(FILE* fh)  { close(); m_fh = fh; }
  void set(int fd)    { close(); m_fd = fd; }

private:
  FILE* m_fh;
  int m_fd;
  NXLFSFile* m_ff;
};



template <typename X>
class auto_delete
{
public:
  auto_delete(X* ptr = NULL) { m_ptr = ptr; }
  ~auto_delete() { delete m_ptr; m_ptr = NULL; }
  X* set(X* ptr) { delete m_ptr; m_ptr = ptr; return m_ptr; }
  X* release() { X* r = m_ptr; m_ptr = NULL; return r; }
  X* ptr() const { return m_ptr; }

  X* operator -> () const { return m_ptr; }
  operator X* () const { return m_ptr; }

private:
  X* m_ptr;
};




template <typename X>
class auto_deletev
{
public:
  auto_deletev(X** ptr) : m_ptr(ptr) { }
  ~auto_deletev() { reset(true); }
  void reset(bool del = true) { if(m_ptr) { if(del) delete *m_ptr; *m_ptr = NULL; } }
  void release() { m_ptr = NULL; }

private:
  X** m_ptr;
};



template <typename X>
class auto_freev
{
public:
  auto_freev(X** ptr) : m_ptr(ptr) { }
  ~auto_freev() { reset(true); }
  void reset(bool fre = true) { if(m_ptr) { if(fre) NXfree(*m_ptr); *m_ptr = NULL; } }
  void release() { m_ptr = NULL; }

private:
  X** m_ptr;
};
