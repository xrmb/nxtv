#pragma once



class NXFile
{
public:
  NXFile();
  ~NXFile();

  int load(const wchar_t* path);
  int load(const char* path);
  int parse(const char* data, size_t len);
  int parse(char* data, size_t len);

  bool pair(const char*& key, const char*& value);
  const char* find(const char* key, const char* value = 0 /*NULL*/);
  size_t tell() const { return m_pos-m_buf; }
  int seek(size_t pos);

private:
  int load(int fd);
  int parse1();
  int parse2();
  void cleanup();

  char* m_buf;
  char* m_pos;
  size_t m_len;
};
