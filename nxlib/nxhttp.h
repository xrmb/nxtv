#pragma once


class NXHTTP
{
public:
  NXHTTP();
  ~NXHTTP();

  int get(const char* uri);

  const char* header() const;
  const char* body() const;
  int bodysize() const;

private:
  int connect(const char* host);
  int disconnect();
  int read();
  int write(const char* data);

  int fetch(const char* uri);
  int store(const char* uri);

  SOCKET m_socket;
  fd_set m_ss;
  timeval m_t;

  char* m_buf;
  size_t m_size;
  size_t m_body;
  size_t m_rlen;
};
