#include "crc32.h"
#include <string.h>



bool CRC32::m_tablebuilt = false;
unsigned __int32 CRC32::m_table[0x100];



void CRC32::buildtable()
{
  int i, j;
  for(i = 0; i < 0x100; ++i)
  {
    m_table[i] = reflect(static_cast<unsigned __int32>(i), 8) << 24;

    for(j = 0; j < 8; ++j)
      m_table[i] = (m_table[i] << 1) ^ ( (m_table[i] & (1<<31))  ? POLYNOMIAL : 0);

    m_table[i] = reflect(m_table[i], 32);
  }
  m_tablebuilt = true;
}



unsigned __int32 CRC32::reflect(unsigned __int32 v, int bits)
{
  unsigned __int32 ret = 0;
  int i;

  --bits;
  for(i = 0; i <= bits; ++i)
  {
    if(v & (1<<i))
      ret |= 1 << (bits-i);
  }
  return ret;
}



void CRC32::hash(const void* buf, size_t size)
{
  if(buf == NULL) return;

  const unsigned __int8* p = reinterpret_cast<const unsigned __int8*>(buf);

  if(!m_tablebuilt)
    buildtable();

  size_t i;
  for(i = 0; i < size; ++i)
    m_crc = (m_crc >> 8) ^ m_table[ (m_crc & 0xFF) ^ p[i] ];
}



void CRC32::hash(const char* buf)
{
  if(buf == NULL) return;
  hash(buf, strlen(buf));
}



void CRC32::hash(const wchar_t* buf)
{
  if(buf == NULL) return;
  hash(buf, wcslen(buf));
}
