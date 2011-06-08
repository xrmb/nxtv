#pragma once

#include "nxlib.h"

const int NXNNTPCRC = 1;  // 10% of cpu goes here

class NXNNTP
{
public:
  struct Post
  {
    char msgid[NXMAXMSGID];
    char filename[MAX_PATH];
    char group[MAX_PATH];
    char from[MAX_PATH];
    char subject[MAX_PATH];
    int part;
    int parts;
    int lines;
    //unsigned __int32 crc;
    size_t len;
    size_t begin;
    size_t tlen;
    char* data;
  };

  NXNNTP(int cid = 0);
  ~NXNNTP();

  int get(const char* msgid, int oid = -1);
  int aget(const char* msgid);
  int tget();
  int agret();

  int head(const char* msgid);
  int ahead(const char* msgid);
  int thead();
  int ahret();

  int mhead();
  int amhead(const char** msgid, int count);
  int tmhead();
  int amhret(int* r = NULL, int count = 0);

  int post(Post& post);

  int cid() const { return m_cid; }
  int ydecode(NXPart* data, size_t datalen, char* buf, size_t len);
  int yencode(char* dest, size_t destlen, Post& post);
  int busy(const char* msgid = NULL) const;
  int idledisconnect();
  bool connected() const { return m_socket != INVALID_SOCKET; }
  void giveup() { m_giveup = true; }
  int mkmsgid(char* dest, size_t len);


private:
  void rebuf(size_t rloc = 0, size_t wloc = 0) { m_rloc = rloc; m_wloc = wloc; m_buf[rloc] = '\0'; m_buf[wloc] = '\0'; }

  int connect();
  int disconnect(int err);

  int readline();
  int readtodot();
  int command(const char* cmd);

  char* ymeta(char* buf, const char* tag, bool restore = true);

  SOCKET m_socket;
  sockaddr_in m_cs;

  size_t m_size;
  char* m_buf;
  char* m_cmd;
  size_t m_rloc;
  size_t m_wloc;
  size_t m_cmds;

  fd_set m_ss;
  timeval m_t;

  char m_agmsgid[NXMAXMSGID];
  int m_agret;
  int m_agoid;

  char m_ahmsgid[NXMAXMSGID];
  int m_ahret;

  char m_amhmsgid[NXMHEAD+1][NXMAXMSGID];
  int m_amhret;
  int m_amhrets[NXMHEAD];

  int m_cid;                      // connection id
  int m_err;
  time_t m_time;
  bool m_giveup;
};
