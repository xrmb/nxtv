#pragma once

class CRC32
{
public:
  CRC32() { reset(); }
  CRC32(const void* buf, size_t size) { reset(); hash(buf, size); }

  operator unsigned __int32 () const { return ~m_crc; }

  unsigned __int32 get() const { return ~m_crc; }

  void reset() { m_crc = ~0; }
  void hash(const void* buf, size_t size);
  void hash(const char* buf);
  void hash(const wchar_t* buf);

private:
  unsigned __int32 m_crc;
  static bool m_tablebuilt;
  static unsigned __int32  m_table[0x100];

  static const unsigned __int32 POLYNOMIAL = 0x04C11DB7;

private:
  static void buildtable();
  static unsigned __int32 reflect(unsigned __int32 v, int bits);
};
