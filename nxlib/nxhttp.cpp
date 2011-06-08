#include "nxlib.h"
#include "nxhttp.h"
#include "crc32.h"



NXHTTP::NXHTTP()
: m_buf(NULL),
  m_size(0),
  m_body(0),
  m_rlen(0)
{
  m_socket = INVALID_SOCKET;

  FD_ZERO(&m_ss);

  m_t.tv_sec = 10;
  m_t.tv_usec = 0;

}



NXHTTP::~NXHTTP()
{
  NXfree(m_buf); m_buf = NULL;
  m_size = 0;
}



int NXHTTP::connect(const char* host)
{
  addrinfo* result = NULL;
  addrinfo hints;
  sockaddr_in cs;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  int r = getaddrinfo(host, "80", &hints, &result);
  if(r) return R(-1, "getaddrinfo failed with error: %d", r);

  addrinfo* ptr;
  bool ok = false;
  for(ptr = result; ptr != NULL ; ptr = ptr->ai_next) 
  {
    if(ptr->ai_family != AF_INET) continue;
    memcpy(&cs, ptr->ai_addr, sizeof(sockaddr_in));
    ok = true;
    break;
  }
  freeaddrinfo(result);
  if(!ok) return R(-2, "no valid addrinfo found");

  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(m_socket == INVALID_SOCKET) return R(-3, "socket failed with error: %d", WSAGetLastError());


  r = ::connect(m_socket, (SOCKADDR*)&cs, sizeof(cs));
  if(r == SOCKET_ERROR) 
  {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return R(-4, "connect failed with error: %d", WSAGetLastError());
  }

  FD_ZERO(&m_ss);
  FD_SET(m_socket, &m_ss);

  return 0;
}



int NXHTTP::disconnect()
{
  int ret = 0;

  FD_ZERO(&m_ss);

  int r = shutdown(m_socket, 2/*SD_BOTH*/);
  if(r == SOCKET_ERROR) 
  {
    if(!ret) ret = R(-1, "shutdown failed with error: %d", WSAGetLastError());
  }

  r = closesocket(m_socket);
  if(r == SOCKET_ERROR) 
  {
    if(!ret) r = R(-2, "closesocket failed with error: %d", WSAGetLastError());
  }

  m_socket = INVALID_SOCKET;

  return ret;
}



int NXHTTP::write(const char* data)
{
  int l = strlen(data);
  int r = send(m_socket, data, l, 0);
  if(r == SOCKET_ERROR)
  {
    disconnect();
    return R(-1, "send failed with error: %d", WSAGetLastError());
  }
  if(r != l)
  {
    disconnect();
    return R(-2, "send(%d) vs sent(%d) error", l, r);
  }
  return r;
}



int NXHTTP::read()
{
  int r;
  size_t wloc = 0;
  for(;;)
  {
    if(m_size - wloc < 4096)
    {
      if(m_size > NXMAXHBLOCKSIZE) return R(-1, "the buffer is too damn full!");
      m_buf = (char*)NXrealloc(m_buf, m_size + NXHBLOCKSIZE);
      if(!m_buf) { m_size = 0; return R(-2, "buffer realloc error"); }
      m_size += NXHBLOCKSIZE;
    }

    r = select(0, &m_ss, NULL, NULL, &m_t);
    if(r == SOCKET_ERROR)
    {
      disconnect();
      return R(-3, "select failed with error: %d", WSAGetLastError());
    }

    if(r == 0)
    {
      disconnect();
      return R(-4, "select timeout");
    }

    r = recv(m_socket, m_buf + wloc, m_size - wloc -1, 0); // -1 because we want to add \0
    if(r == SOCKET_ERROR)
    {
      disconnect();
      return R(-5, "recv failed with error: %d", WSAGetLastError());
    }

    if(r == 0) 
    {
      return wloc;
    }

    wloc += r;
    m_buf[wloc] = '\0';
  }
}



int NXHTTP::get(const char* uri)
{
  m_rlen = 0;
  m_body = 0;

  if(strncmp(uri, "http://", 7) != 0) return R(-1, "protocol not supported");

  char* host = strdupa(uri+7);
  char* path = strchr(host, '/');
  if(path == NULL) return R(-2, "no path");
  path[0] = '\0';
  path++;


  int r = fetch(uri);
  if(r < 0) return R(-3, "fetch error");
  if(r == 0) return 0;


  if(connect(host)) return R(-4, "connect error");

  if(m_size < 4096)
  {
    if(m_size > NXMAXHBLOCKSIZE) return R(-5, "the buffer is too damn full!");
    m_buf = (char*)NXrealloc(m_buf, m_size + NXHBLOCKSIZE);
    if(!m_buf) { m_size = 0; return R(-6, "buffer realloc error"); }
    m_size += NXHBLOCKSIZE;
  }

  if(sprintf_s(m_buf, m_size, 
      "GET /%s HTTP/1.0\r\n"
      "Host: %s\r\n"
      "Connection: close\r\n"
      "User-Agent: nxlib build %d\r\n"
      "\r\n", path, host, NXLIBBUILD) == -1) return R(-11, "buffer error");

  r = write(m_buf);
  if(r <= 0) return R(-12, "write error");

  m_rlen = read();
  if(m_rlen <= 0) return R(-101, "read error");

  disconnect();

  if(r <= 16) return R(-201, "response to short");
  if(strncmp(m_buf, "HTTP/1.0", 8) != 0 && strncmp(m_buf, "HTTP/1.1", 8) != 0) return R(-202, "no http response");
  int code = atol(m_buf+9);
  if(code <= 0 || code == LONG_MAX) return R(-203, "invalid http code");
  if(code != 200) return R(-204, "http response %d not supported", code);

  char* body = strstr(m_buf, "\r\n\r\n");
  if(!body) return R(-211, "no response body");
  m_body = body - m_buf + 4;

  store(uri);

  return 0;
}



const char* NXHTTP::header() const
{
  if(!m_rlen) { R(-1, "no valid request"); return NULL; }

  return m_buf;
}



const char* NXHTTP::body() const
{
  if(!m_rlen) { R(-1, "no valid request"); return NULL; }

  return m_buf+m_body;
}



int NXHTTP::bodysize() const
{
  if(!m_rlen) { R(-1, "no valid request"); return 0; }

  return m_rlen-m_body;
}



int NXHTTP::fetch(const char* uri)
{
  if(!NXLFSConfigNews::i()->cache()) return 1;  // ok, no cache

  unsigned __int32 crc = CRC32(uri, strlen(uri));
  char p[MAX_PATH];
  if(sprintf_s(p, MAX_PATH, "%s\\%s\\%X\\%08X", NXLFSConfigNews::i()->cache(), "rem", (crc & 0xF0000000) >> 28, crc) == -1) { return R(-1, "buffer error"); }

  int fd = -1;
  if(_sopen_s(&fd, p, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IWRITE)) return 2; // ok, not there
  if(fd == -1) return 3; // ok, but not really ;)

  auto_close acfd(fd);

  struct stat st;
  if(fstat(fd, &st)) return R(-2, "stat error");
  size_t size = st.st_size;

  auto_free<char> buf(size);
  if(size != _read(fd, buf, size)) return R(-3, "read error");

  acfd.close();

  char* body = strstr(buf, "\r\n\r\n");
  if(!body) return R(-4, "no response body");

  m_buf = buf.release();
  m_body = body - m_buf + 4;
  m_size = m_rlen = size;

  return 0;
}



int NXHTTP::store(const char* uri)
{
  if(!NXLFSConfigNews::i()->cache()) return 0;

  unsigned __int32 crc = CRC32(uri, strlen(uri));
  char p[MAX_PATH];
  if(sprintf_s(p, MAX_PATH, "%s\\%s\\%X\\%08X", NXLFSConfigNews::i()->cache(), "rem", (crc & 0xF0000000) >> 28, crc) == -1) { return R(-1, "buffer error"); }

  int fd = -1;
  if(_sopen_s(&fd, p, _O_BINARY | _O_WRONLY | _O_CREAT, _SH_DENYWR, _S_IWRITE)) return R(-2, "open error");
  if(fd == -1) return R(-12, "no file descriptor");

  if(m_rlen != _write(fd, m_buf, m_rlen))
  {
    _close(fd);
    return R(-13, "write error");
  }
  _close(fd); // ERR

  return 0;
}
