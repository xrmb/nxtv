#include "nxlib.h"

#include "crc32.h"



NXNNTP::NXNNTP(int cid)
{
  m_socket = INVALID_SOCKET;

  m_size = NXBLOCKSIZE; 
  m_buf = (char*)NXmalloc(m_size);
  m_rloc = 0;
  m_wloc = 0;

  m_cmds = NXMHEAD*(4+1+1+NXMAXMSGID+1+2);
  m_cmd = (char*)NXmalloc(m_cmds);

  FD_ZERO(&m_ss);

  m_t.tv_sec = 10;
  m_t.tv_usec = 0;

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  if(cfg)
    m_t.tv_sec = cfg->timeout();

  m_cid = cid;

  m_agmsgid[0] = '\0';
  m_agret = -1;
  m_agoid = 0;

  m_ahmsgid[0] = '\0';
  m_ahret = -1;

  m_amhmsgid[0][0] = '\0';
  m_amhret = -1;

  m_err = 0;
  m_giveup = false;
  m_time = 0;
}



NXNNTP::~NXNNTP()
{
  disconnect(0);

  NXfree(m_buf); m_buf = NULL;
  NXfree(m_cmd); m_cmds = NULL;
  
  m_size = 0;
  m_cmds = 0;
}



int NXNNTP::connect()
{
  if(m_socket != INVALID_SOCKET) return 0;

  disconnect(0);

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  if(!cfg) return R(-1, "no config");

  if(m_err) Sleep(min(5, m_err) * 1000);

  addrinfo* result = NULL;
  addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  int r = getaddrinfo(cfg->host(), "119", &hints, &result);
  if(r) return R(-1002, "%02d getaddrinfo failed with error: %d", m_cid, r);

  addrinfo* ptr;
  bool ok = false;
  for(ptr = result; ptr != NULL ; ptr = ptr->ai_next) 
  {
    if(ptr->ai_family != AF_INET) continue;
/*
    m_cs.sin_family = AF_INET;
    m_cs.sin_addr.s_addr = inet_addr("216.151.153.40");
    m_cs.sin_port = htons(cfg->port());

    sockaddr_in cs;
    memcpy(&cs, ptr->ai_addr, sizeof(sockaddr_in));
*/
    memcpy(&m_cs, ptr->ai_addr, sizeof(sockaddr_in));
    ok = true;
    break;
  }
  if(!ok) return R(-1003, "no valid addrinfo found");
  freeaddrinfo(result);

  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(m_socket == INVALID_SOCKET) return R(-1004, "%02d socket failed with error: %d", m_cid, WSAGetLastError());


  r = ::connect(m_socket, (SOCKADDR*)&m_cs, sizeof(m_cs));
  if(r == SOCKET_ERROR) 
  {
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    return R(-1005, "%02d connect failed with error: %d", m_cid, WSAGetLastError());
  }

  FD_ZERO(&m_ss);
  FD_SET(m_socket, &m_ss);


  rebuf();
  r = readline();
  if(r <= -1000) return R(-1011, "readline error");
  if(r < 0) return R(-11, "readline error");
  if(r != 200)
  {
    disconnect(1);
    return R(-1012, "%02d not welcome? code: %d", m_cid, r);
  }

  if(cfg->user())
  {
    if(sprintf_s(m_cmd, m_cmds, "authinfo user %s\r\n", cfg->user()) == -1) return R(-21, "buffer error");
    r = command(m_cmd);
    if(r <= -1000) return R(-1022, "command error");
    if(r < 0) return R(-22, "command error");

    rebuf();
    r = readline();
    if(r <= -1000) return R(-1023, "readline error");
    if(r < 0) return R(-23, "readline error");
    if(r != 381)
    {
      disconnect(1);
      return R(-1024, "%02d what now after user? code: %d", m_cid, r);
    }

    if(sprintf_s(m_cmd, m_cmds, "authinfo pass %s\r\n", cfg->pass()) == -1) return R(-31, "buffer error");
    r = command(m_cmd);
    if(r <= -1000) return R(-1032, "command error");
    if(r < 0) return R(-32, "command error");

    rebuf();
    r = readline();
    if(r <= -1000) return R(-1033, "readline error");
    if(r < 0) return R(-33, "readline error");
    if(r != 281)
    {
      disconnect(1);
      return R(-1034, "%02d password rejected? code: %d", m_cid, r);
    }
  }

  return 0;
}



int NXNNTP::disconnect(int err)
{
  if(m_socket == INVALID_SOCKET) return 0;
  if(err) m_err++;

  int ret = 0;
  if(!err && command("quit\r\n") > 0)
  {
    rebuf();
    int code = readline();
    if(code <= -1000) ret = R(-1001, "readline error");
    else if(code < 0) ret = R(-2, "readline error");
    else if(code != 205)
    {
      if(!ret) ret = R(-1003, "%02d quit not ok? code: %d", m_cid, code);
    }
  }

  FD_ZERO(&m_ss);

  int r = shutdown(m_socket, 2/*SD_BOTH*/);
  if(r == SOCKET_ERROR) 
  {
    if(!ret) ret = R(-1004, "%02d shutdown failed with error: %d", m_cid, WSAGetLastError());
  }

  r = closesocket(m_socket);
  if(r == SOCKET_ERROR) 
  {
    if(!ret) r = R(-1005, "%02d closesocket failed with error: %d", m_cid, WSAGetLastError());
  }

  m_socket = INVALID_SOCKET;

  return r;
}



int NXNNTP::readline()
{
  int r;
  for(;;)
  {
    const char* eol = strstr(m_buf + m_rloc, "\r\n");
    if(eol)
    {
      const char* d = m_buf + m_rloc;
      m_rloc = eol - m_buf + 2;
      if(!isdigit(d[0]) || !isdigit(d[1]) || !isdigit(d[2]))
      {
        disconnect(1);
        return R(-1001, "%02d line has no code", m_cid);
      }
      r = (d[0]-'0') * 100 + (d[1]-'0') * 10 + (d[2]-'0') * 1;
      return r;
    }

    if(m_size - m_wloc < 4096)
    {
      if(m_size > NXMAXBLOCKSIZE) return R(-2, "%02d the buffer is too damn full!", m_cid);
      //printf("%02d growing buffer to %08d\n", m_cid, m_size + NXBLOCKSIZE);
      m_buf = (char*)NXrealloc(m_buf, m_size + NXBLOCKSIZE);
      if(!m_buf) { m_size = m_rloc = m_wloc = 0; return R(-3, "%02d buffer realloc error", m_cid); }
      m_size += NXBLOCKSIZE;
    }

    r = select(0, &m_ss, NULL, NULL, &m_t);
    if(r == SOCKET_ERROR)
    {
      disconnect(1);
      return R(-1004, "%02d select failed with error: %d", m_cid, WSAGetLastError());
    }

    if(r == 0)
    {
      disconnect(1);
      return R(-1005, "%02d select timeout", m_cid);
    }

    r = recv(m_socket, m_buf + m_wloc, m_size - m_wloc -1, 0); // -1 because we want to add \0
    if(r == SOCKET_ERROR)
    {
      disconnect(1);
      return R(-1006, "%02d recv failed with error: %d", m_cid, WSAGetLastError());
    }

    if(r == 0)
    {
      disconnect(1);
      return R(-1007, "%02d recv no data", m_cid);
    }

    m_wloc += r;
    m_buf[m_wloc] = '\0';
  }
}



int NXNNTP::command(const char* cmd)
{
  int l = strlen(cmd);
  int r = send(m_socket, cmd, l, 0);
  if(r == SOCKET_ERROR)
  {
    disconnect(1);
    return R(-1001, "%02d send failed with error: %d", m_cid, WSAGetLastError());
  }
  if(r != l)
  {
    disconnect(1);
    return R(-1002, "%02d send(%d) vs sent(%d) error", m_cid, l, r);
  }
  return r;
}



int NXNNTP::get(const char* msgid, int oid)
{
  for(;;)
  {
    if(m_giveup) return R(-1, "giving up");
    if(msgid == NULL) return R(-1, "no message id");

    int r = connect();
    if(r <= -1000) continue;
    if(r) return R(-2, "connect error");

    //printf("%02d grab <%s>\n", m_cid, msgid);
    NXLFSMessageCache* cache = NXLFSMessageCache::i();
    if(!cache) return R(-3, "no message cache");
    if(oid == -1) oid = cache->oid();

    if(sprintf_s(m_cmd, m_cmds, "body <%s>\r\n", msgid) == -1) return R(-4, "buffer error");
    r = command(m_cmd);
    if(r <= -1000) continue;
    if(r < 0) return R(-5, "command error");

    rebuf(sizeof(NXPart), sizeof(NXPart));
    r = readline();
    if(r <= -1000) continue;
    if(r < 0) return R(-6, "readline error");
    if(r != 222) return R(-6, "%02d body problem? code: %d msgid: %s", m_cid, r, msgid);

    r = readtodot();
    if(r <= -1000) continue;
    if(r <= 0) return R(-7, "readtodot error");

    NXPart* data = (NXPart*)m_buf;
    memset(data, 0, sizeof(NXPart));
    int ylen = ydecode(data, m_size - sizeof(NXPart), m_buf + m_rloc - r, r);
    if(ylen <= 0) return R(-8, "yenc decode error");

    if(strcpy_s(data->msgid, NXMAXMSGID, msgid)) return R(-9, "buffer error");
    cache->store(data, oid);

    m_err = 0;
    m_time = time(NULL);
    return 0;
  }
}



int NXNNTP::head(const char* msgid)
{
  for(;;)
  {
    if(m_giveup) return R(-1, "giving up");
    if(msgid == NULL) return R(-1, "no message id");

    int r = connect();
    if(r <= -1000) continue;
    if(r) return R(-2, "connect error");

    if(sprintf_s(m_cmd, m_cmds, "head <%s>\r\n", msgid) == -1) return R(-3, "buffer error");
    r = command(m_cmd);
    if(r <= -1000) continue;
    if(r < 0) return R(-4, "command error");

    rebuf();
    r = readline();
    if(r <= -1000) continue;
    if(r < 0) return R(-5, "readline error");
    if(r != 221) return R(-5, "%02d head problem? code: %d msgid: %s", m_cid, r, msgid);

    r = readtodot();
    if(r <= -1000) continue;
    if(r <= 0) return R(-6, "readtodot error");

    m_err = 0;
    m_time = time(NULL);
    return 0;
  }
}



int NXNNTP::mhead()
{
  for(;;)
  {
    if(m_giveup) return R(-1, "giving up");
    //if(msgid == NULL) return R(-1, "no message id");

    int r = connect();
    if(r <= -1000) continue;
    if(r) return R(-2, "connect error");

    size_t o = 0;
    for(int i = 0; i < NXMHEAD && m_amhmsgid[i][0]; i++)
    {
      m_amhrets[i] = 0;
      int o0 = sprintf_s(m_cmd+o, m_cmds-o, "head <%s>\r\n", m_amhmsgid[i]);
      if(o0 == -1) return R(-3, "buffer error");
      o += o0;
    }
    r = command(m_cmd);
    if(r <= -1000) continue;
    if(r < 0) return R(-4, "command error");

    rebuf();
    bool c = false;
    for(int i = 0; i < NXMHEAD && m_amhmsgid[i][0]; i++)
    {
      r = readline();
      if(r <= -1000) { c = true; break; }
      if(r < 0) return R(-5, "readline error");
      if(r != 221) 
      {
        m_amhrets[i] = R(-5, "%02d head problem? code: %d msgid %s", m_cid, r, m_amhmsgid[i]);
        continue;
      }

      r = readtodot();
      if(r <= -1000) { c = true; break; }
      if(r <= 0) return R(-6, "readtodot error");
    }
    if(c) continue;

    m_err = 0;
    m_time = time(NULL);
    return 0;
  }
}



/*
  return <=0 on error, < -1000 might be recovered by reconnect
  returns number of bytes at m_buf+m_rloc
*/
int NXNNTP::readtodot()
{
  int r;
  size_t sloc = m_rloc + 5;
  for(;;)
  {
    if(m_wloc > 5)
    {
      //if(strcmp(m_buf + m_wloc - 5, "\r\n.\r\n") == 0)
      //const char* dot = strstr(m_buf+m_rloc, "\r\n.\r\n");
      const char* dot = strstr(m_buf+sloc-5, "\r\n.\r\n");
      if(dot)
      {
        size_t rloc = m_rloc;
        m_rloc = dot - m_buf + 5;
        return m_rloc - rloc;
      }
      sloc = m_wloc;
    }

    if(m_size - m_wloc < 4096)
    {
      if(m_size > NXMAXBLOCKSIZE) return R(-1, "%02d the buffer is too damn full!", m_cid);
      //printf("%02d growing buffer to %08d\n", m_cid, m_size + NXBLOCKSIZE);
      m_buf = (char*)NXrealloc(m_buf, m_size + NXBLOCKSIZE);
      if(!m_buf) { m_size = m_rloc = m_wloc = 0; return R(-2, "%02d buffer realloc error", m_cid); }
      m_size += NXBLOCKSIZE;
    }

    r = select(0, &m_ss, NULL, NULL, &m_t);
    if(r == SOCKET_ERROR)
    {
      disconnect(1);
      return R(-1003, "%02d select failed with error: %d", m_cid, WSAGetLastError());
    }

    if(r == 0)
    {
      disconnect(1);
      return R(-1004, "%02d select timeout", m_cid);
    }

    r = recv(m_socket, m_buf + m_wloc, m_size - m_wloc -1, 0); // -1 because we want to add \0
    if(r == SOCKET_ERROR)
    {
      disconnect(1);
      return R(-1005, "%02d recv failed with error: %d", m_cid, WSAGetLastError());
    }

    if(r == 0) 
    {
      disconnect(1);
      return R(-1006, "%02d recv no data", m_cid);
    }

    m_wloc += r;
    m_buf[m_wloc] = '\0';
  }
}



int NXNNTP::ydecode(NXPart* data, size_t datasize, char* buf, size_t len)
{
  if(len <= 0) return R(-1, "invalid length");

  char* end;
  char* pos;
  char* line;
  char* eol;

  line = buf;
  for(;;)
  {
    eol = strstr(line, "\r\n");
    if(!eol) return R(-1, "no eol");
    if(eol == line) 
    {
      line += 2;
      continue;
    }
    *eol = '\0';
    break;
  }

  if(strstr(line, "=ybegin ") == NULL) return R(-2, "no ybegin");

  pos = ymeta(line, "size=", false);
  if(!pos) return R(-3, "no size");

  __int64 ysize = _strtoi64(pos, &end, 10);
  if(pos == end || ysize <= 0 || ysize == _I64_MAX) return R(-4, "invalid size");

  pos = ymeta(line, "part=");
  if(!pos) return R(-5, "no part");

  int ypart = strtol(pos, &end, 10);
  if(pos == end || ypart < 1 || ypart == LONG_MAX) return R(-6, "invalid part");

  line = eol+2;
  eol = strstr(eol+2, "\r\n");
  if(!eol) return R(-11, "no eol");
  *eol = '\0';


  if(strncmp(line, "=ypart ", 7) != 0)  return R(-21, "todo");
  pos = ymeta(line, "begin=", false);
  if(!pos) return R(-22, "todo");
  __int64 ybegin = _strtoi64(pos, &end, 10);
  if(pos == end || ybegin <= 0 || ybegin == _I64_MAX) return R(-23, "todo");

  pos = ymeta(line, "end=");
  if(!pos) return R(-24, "todo");
  __int64 yend = _strtoi64(pos, &end, 10);
  if(pos == end || yend <= 0 || yend == _I64_MAX || ybegin >= yend) return R(-25, "todo");

  int yblocksize;
  if(ypart == 1 && ybegin == 1 && yend == ysize)
  {
    if(yend - ybegin > m_size) return R(-26, "todo");
    yblocksize = (int)(yend - ybegin + 1);
  }
  else
  {
    if(ypart == 1)
    {
      yblocksize = (int)yend;
    }
    else
    {
      if(((ybegin - 1) / (ypart - 1)) * (ypart -1 ) != ybegin - 1)  return R(-27, "todo");
      if(yend - ybegin + 1 > datasize) return R(-28, "todo");
      yblocksize = (int)((ybegin - 1) / (ypart - 1));
    }
  }

  char* rpos = eol+2;
  char* wpos = &data->data;

  bool first = true;
  for(;;)
  {
    if(rpos >= buf+len - 8) // 8 for "\r\n=yenc "
    {
      return R(-31, "todo");
    }

    if(first)
    {
      first = false;
      if(rpos[0] == '.')
      {
        rpos += 2;
        wpos[0] = '.' - 42;
        wpos++;
        continue;
      }
    }

    if(rpos[0] == '\r' && rpos[1] == '\n')
    {
      first = true;
      rpos += 2;
      if(strncmp(rpos, "=yend ", 6) == 0) break;
      continue;
    }

    if(rpos[0] == '=')
    {
      rpos++;
      rpos[0] = rpos[0] >= 64 ? rpos[0] - 64 : rpos[0] + 192;
    }

    wpos[0] = rpos[0] >= 42 ? rpos[0] - 42 : rpos[0] + 214;
    wpos++;
    rpos++;
  }

  line = rpos;
  eol = strstr(rpos, "\r\n");
  if(!eol) return R(-41, "todo");
  *eol = '\0';

  pos = ymeta(line, "part=", false);
  if(!pos) return R(-42, "todo");
  int ypart2 = strtol(pos, &end, 10);
  if(pos == end || ypart2 < 1 || ypart2 == LONG_MAX) return R(-43, "todo");
  if(ypart != ypart2) return R(-44, "todo");


  pos = ymeta(line, "pcrc32=");
  if(!pos) return R(-45, "todo");
  unsigned __int32 ycrc32 = strtoul(pos, &end, 16);
  if(pos == end || ycrc32 < 1 || ycrc32 == ULONG_MAX) return R(-46, "todo");


  pos = ymeta(line, "size=");
  if(!pos) return R(-47, "todo");
  int size = strtol(pos, &end, 10);
  if(pos == end || size < 1 || size == ULONG_MAX) return R(-48, "todo");

  if(NXNNTPCRC)
  {
    unsigned __int32 crc32 = CRC32(&data->data, size);
    if(ycrc32 != crc32) return R(-51, "ydecode crc error");
  }
  if(wpos - &data->data != size) return R(-52, "ydecode size error");

  data->version = NXPARTVERSION;
  data->size = ysize;
  data->blocksize = yblocksize;
  data->crc32 = ycrc32;
  data->len = size;
  data->begin = ybegin-1; /// lets fix that stupid +1 offset right here

  return size;
}



char* NXNNTP::ymeta(char* buf, const char* tag, bool restore)
{
  if(restore)
  {
    buf[strlen(buf)] = ' ';
  }
  char* pos = strstr(buf, tag);
  if(pos == NULL) return NULL;

  char term = ' ';
  char* end = strchr(pos, term);
  if(end) *end = '\0';

  return pos+strlen(tag);
}



int NXNNTP::aget(const char* msgid)
{
  if(m_giveup) return R(-1, "giving up");
  if(m_agmsgid[0]) return R(-2, "aget running");
  if(msgid == NULL) return R(-3, "no msgid");
  
  NXLFSMessageCache* cache = NXLFSMessageCache::i();
  if(!cache) return R(-4, "no cache");

  m_agoid = cache->oid();
  if(strcpy_s(m_agmsgid, NXMAXMSGID, msgid)) return R(-5, "buffer error");
  m_agret = -1;

  return 0;
}



int NXNNTP::tget()
{
  m_agret = -2;
  if(m_agmsgid[0])
  {
    m_agret = get(m_agmsgid, m_agoid);
    m_agmsgid[0] = '\0';
  }

  return m_agret;
}



int NXNNTP::agret() 
{ 
  return m_agret; 
}



int NXNNTP::ahead(const char* msgid)
{
  if(m_giveup) return R(-1, "giving up");
  if(m_ahmsgid[0]) return R(-2, "ahead running");
  if(msgid == NULL) return R(-3, "no msgid");

  strcpy_s(m_ahmsgid, NXMAXMSGID, msgid);
  m_ahret = -1;

  return 0;
}



int NXNNTP::amhead(const char** msgid, int count)
{
  if(m_giveup) return R(-1, "giving up");
  if(m_amhmsgid[0][0]) return R(-2, "amhead running");
  for(int i = 0; i < min(NXMHEAD, count); i++)
  {
    if(msgid[i] == NULL) return R(-3, "no msgid");
  }

  if(count > NXMHEAD) return R(-4, "max mhead");
  for(int i = 0; i < min(NXMHEAD, count); i++)
  {
    strcpy_s(m_amhmsgid[i], NXMAXMSGID, msgid[i]);
  }
  m_amhmsgid[count][0] = '\0';

  m_amhret = -1;

  return 0;
}



int NXNNTP::thead()
{
  m_ahret = -2;
  if(m_ahmsgid[0])
  {
    m_ahret = head(m_ahmsgid);
    m_ahmsgid[0] = '\0';
  }

  return m_ahret;
}



int NXNNTP::tmhead()
{
  if(m_amhmsgid[0][0])
  {
    m_amhret = mhead();
    m_amhmsgid[0][0] = '\0';
  }

  return m_amhret;
}



int NXNNTP::ahret() 
{ 
  return m_ahret; 
}



int NXNNTP::amhret(int* r, int count) 
{ 
  if(r)
  {
    for(int i = 0; i < min(NXMHEAD, count); i++)
    {
      if(m_amhret)
        r[i] = m_amhret;
      else
        r[i] = m_amhrets[i];
    }
  }
  return m_amhret; 
}



int NXNNTP::busy(const char* msgid) const 
{  
  if(msgid) 
  {
    if(strcmp(m_agmsgid, msgid) == 0) return 1;
    if(strcmp(m_ahmsgid, msgid) == 0) return 2;
    return 0;
  }
  
  if(m_agmsgid[0]) return 1;
  if(m_ahmsgid[0]) return 2;
  if(m_amhmsgid[0][0]) return 3;
  return 0; 
}



int NXNNTP::idledisconnect()
{
  if(!connected()) return 0;
  if(busy()) return 0;

  const NXLFSConfigNews* cfg = NXLFSConfigNews::i();
  size_t idle = 60;
  if(cfg)
    idle = cfg->idle();
  size_t t = (size_t)(time(NULL) - m_time);
  if(t > idle)
  {
    printf("%02d idle(%d) disconnect\n", m_cid, t);
    return disconnect(0);
  }

  return 0;
}



int NXNNTP::post(Post& post)
{
  if(!post.from[0]) return R(-1, "no from");
  if(!post.msgid[0]) return R(-1, "no msgid");
  if(!post.subject[0]) return R(-1, "no subject");
  if(!post.group[0]) return R(-1, "no group");
  if(!post.filename[0]) return R(-1, "no filename");

  int r = connect();
  if(r) return R(-1, "connect error");

  r = command("post\r\n");
  if(r < 0) return R(-2, "command error");

  rebuf();
  r = readline();
  if(r < 0) return R(-3, "readline error");
  if(r != 340) return R(-4, "%02d post problem? code: %d", m_cid, r);


  int h = sprintf_s(m_buf, m_size, 
    "From: %s\r\n"
    "Newsgroups: %s\r\n"
    "Subject: %s \"%s\" yEnc (%d/%d)\r\n"
    "Message-ID: <%s>\r\n"
    "\r\n",
    post.from, 
    post.group, 
    post.subject, post.filename, post.parts ? post.part : 1, post.parts ? post.parts : 1,
    post.msgid
    );
  if(h == -1) return R(-11, "buffer error");


  int d = yencode(m_buf+h, m_size-h, post);
  if(d < 0) return R(-12, "yencode error");

  int f = 3;
  if(strcpy_s(m_buf+h+d, m_size-h-d, ".\r\n")) return R(-13, "buffer error");


  r = send(m_socket, m_buf, h+d+f, 0);
  if(r == SOCKET_ERROR)
  {
    disconnect(1);
    return R(-21, "%02d send failed with error: %d", m_cid, WSAGetLastError());
  }
  if(r != h+d+f)
  {
    disconnect(1);
    return R(-22, "%02d send(%d) vs sent(%d) error", m_cid, h+d+f, r);
  }


  rebuf();
  r = readline();
  if(r < 0) return R(-31, "readline error");
  if(r != 240) return R(-32, "%02d post problem? code: %d", m_cid, r);

  return h+d+f;
}



int NXNNTP::yencode(char* dest, size_t destlen, Post& post)
{
  size_t pos = 0;
  int r = 0;
  
  if(!post.filename[0]) return R(-1, "no filename");
  if(!post.data) return R(-2, "no data");
  if(!post.len) return R(-3, "no len");
  if(!post.tlen) return R(-4, "no tlen");
  // todo: more checks: begin+len > tlen; part > parts

  if(!post.part)
  {
    r = sprintf_s(dest+pos, destlen-pos, 
      "=ybegin line=%d size=%d name=%s\r\n",
      NXYENCLINE, post.len, post.filename);
    if(r == -1) return R(-11, "buffer error");
    pos += r; // overwrite \0
    post.lines++;
  }
  else
  {
    r = sprintf_s(dest+pos, destlen-pos, 
      "=ybegin part=%d total=%d line=%d size=%d name=%s\r\n"
      "=ypart begin=%d end=%d\r\n", 
      post.part, post.parts, NXYENCLINE, post.tlen, post.filename,
      post.begin+1, post.begin+post.len);
    if(r == -1) return R(-12, "buffer error");
    pos += r; // overwrite \0
    post.lines += 2;
  }


  char* rpos = post.data;
  int line = 0;
  while(rpos < post.data+post.len)
  {
    if(pos+2*NXYENCLINE > m_size)
    {
      return R(-13, "buffer error");
    }

    char c = rpos[0] > 214 ? rpos[0] - 214 : rpos[0] + 42;
    rpos++;
    if(c == '.' && line == 0)
    {
      dest[pos] = '.';
      pos++;
      line++;
    }
    if(c == '\0' || c == '\r' || c == '\n' || c == '=')
    {
      dest[pos] = '=';
      pos++;
      line++;
      dest[pos] = c > 192 ? c - 192 : c + 64;
      pos++;
      line++;
    }
    else
    {
      dest[pos] = c;
      pos++;
      line++;
    }

    if(line >= NXYENCLINE)
    {
      dest[pos] = '\r';
      pos++;
      dest[pos] = '\n';
      pos++;
      line = 0;
      post.lines++;
    }
  }


  unsigned __int32 crc = CRC32(post.data, post.len);
  if(!post.part)
  {
    r = sprintf_s(dest+pos, destlen-pos, 
      "\r\n"
      "=yend size=%d pcrc32=%08x\r\n",
      post.len, crc);
    if(r == -1) return R(-14, "buffer error");
    pos += r; // overwrite \0
    post.lines += 2;
  }
  else
  {
    r = sprintf_s(dest+pos, destlen-pos, 
      "\r\n"
      "=yend size=%d part=%d pcrc32=%08x\r\n",
      post.len, post.part, crc);
    if(r == -1) return R(-15, "buffer error");
    pos += r; // overwrite \0
    post.lines += 2;
  }

  return pos;
}



int NXNNTP::mkmsgid(char* dest, size_t len)
{
  for(size_t i = 0; i < len-1; i++)
  {
    _itoa(rand()%36, dest+i, 36);
  }

  return 0;
}

