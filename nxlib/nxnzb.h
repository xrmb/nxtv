#pragma once

class NXNzb
{
public:
  NXNzb();
  ~NXNzb();

  int load(const wchar_t* path);
  int parse(const char* data);
  int parse(char* data);
  const char* err() const { return m_err; }

#pragma pack(push)
#pragma pack(1)
  struct Segment
  {
    const char* msgid;
    size_t      bytes;
    long        number;
  };

  struct File
  {
    const char* filename;
    long        time;
    int         segmentc;
    Segment*    segments;
  };
#pragma pack(pop)

  int filec;
  File* files;

  int segmentc;
  Segment* segments;

private:
  int parsei();
  void cleanup();
  int entdec(char* str);
  static int segsort(const void* a, const void* b);

  char* m_buf;
  char* m_err;
};
