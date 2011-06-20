#include "nxlib.h"
#include "nxnzb.h"



NXNzb::NXNzb()
: m_buf(NULL),
  filec(0),
  files(NULL),
  segmentc(0),
  segments(NULL),
  m_err(NXstrdup("no error"))
{
}



NXNzb::~NXNzb()
{
  cleanup();
  NXfree(m_err); m_err = NULL;
}



int NXNzb::load(const wchar_t* path)
{
  int fd = -1;
  if(_wsopen_s(&fd, path, _O_BINARY | _O_RDONLY, _SH_DENYWR, _S_IWRITE)) return RE(-1, "open error");
  if(fd == -1) return RE(-2, "no file descriptor");
  auto_close acfd(fd);

  struct stat st;
  if(fstat(fd, &st)) return RE(-3, "stat error");
  size_t dsize = st.st_size;

  auto_free<char> data(dsize, true);
  if(dsize != _read(fd, data, dsize)) return RE(-4, "read error");
  acfd.close();

  cleanup();
  m_buf = data.release();
  int r = parsei();
  if(r) cleanup();
  return r;
}



int NXNzb::parse(const char* data)
{
  cleanup();
  m_buf = NXstrdup(data);
  int r = parsei();
  if(r) cleanup();
  return r;
}



int NXNzb::parse(char* data)
{
  cleanup();
  m_buf = data;
  int r = parsei();
  if(r) cleanup();
  return r;
}



int NXNzb::parsei()
{
  size_t l = strlen(m_buf);
  char* c = NULL;
  char* e = NULL;
  int f = 0;
  int s = 0;

  //--- count files/segments ---
  c = m_buf;
  while(c = strstr(c+1, "<file ")) filec++;
  if(!filec) return RE(-1, "no file tags");

  c = m_buf;
  while(c = strstr(c+1, "<segment ")) segmentc++;
  if(!segmentc) return RE(-2, "no segment tags");

  files = (File*)NXcalloc(filec, sizeof(File));
  segments = (Segment*)NXcalloc(segmentc, sizeof(Segment));

  char** fs,** fe,** ss,** se;
  auto_free<char*> affs, affe, afss, afse;

  //--- monster nzb's might cause stack overflow ---
  if((filec+segmentc)*2*sizeof(char*) > 256*1024)
  {
    fs = affs.get(filec);
    fe = affe.get(filec);
    ss = afss.get(segmentc);
    se = afse.get(segmentc);
  }
  else
  {
    fs = (char**)NXalloca(sizeof(char*)*filec);
    fe = (char**)NXalloca(sizeof(char*)*filec);
    ss = (char**)NXalloca(sizeof(char*)*segmentc);
    se = (char**)NXalloca(sizeof(char*)*segmentc);
  }

  c = m_buf;
  f = 0;
  while(c = strstr(c+1, "<file ")) 
  {
    fs[f] = c;
    c = strstr(c+10, "</file>");
    if(!c) return RE(-11, "file not closed");
    fe[f] = c;
    f++;
  }

  c = m_buf;
  s = 0;
  while(c = strstr(c+1, "<segment ")) 
  {
    ss[s] = c;
    c = strstr(c+9, "</segment>"); // 9...length of '<segment '
    if(!c) return RE(-12, "segment not closed");
    se[s] = c;
    s++;
  }


  for(f = 0; f < filec; f++)
  {
    fe[f][0] = '\0';

    c = strchr(fs[f], '>');
    if(!c) return RE(-21, "file tag not closed");
    c[0] = '\0';

    c = strstr(fs[f], " date=\"");
    if(!c) return RE(-22, "file missing date attribute");
    c += 7; // length of ' date="'
    e = strchr(c, '"');
    if(!e) return RE(-23, "date attribute no closed");
    e[0] = '\0';
    long time = atol(c); // todo: num/overflow check?
    files[f].time = time;
    e[0] = '"';

    c = strstr(fs[f], " subject=\"");
    if(!c) return RE(-24, "file missing subject attribute");
    c += 10; // length of ' subject="'
    e = strchr(c, '"');
    if(!e) return RE(-25, "subject attribute no closed");
    e[0] = '\0';
    if(entdec(c)) return RE(-26, "entity decode error");
/*
  old way: filename must be in "..."
    char* fn = strrchr(c, '"');
    if(!fn) return RE(-27, "no filename end in subject");
    fn[0] = '\0';
    fn = strrchr(c, '"');
    if(!fn) return RE(-28, "no filename start in subject");
    files[f].filename = fn+1;
*/

    //--- filename in quotes ---
    char* fn = strrchr(c, '"');
    if(fn)
    {
      fn[0] = '\0';
      fn = strrchr(c, '"');
      if(!fn) return RE(-27, "no filename start in subject");
      files[f].filename = fn+1;
    }

    //--- assume "blah blah filename.ext yEnc (1/10)" ---
    else
    {
      for(;;)
      {
        char* fn = strrchr(c, ' ');
        if(!fn) return RE(-28, "no filename end subject");
        fn[0] = '\0';
        fn = strrchr(c, ' ');
        if(fn)
        {
          if(_stricmp(fn+1, "yenc") == 0 || _stricmp(fn+1, "-") == 0)
            continue;
          files[f].filename = fn+1;
        }
        else
          files[f].filename = c;

        break;
      }
    }
    //e[0] = '"';
  }


  for(s = 0; s < segmentc; s++)
  {
    se[s][0] = '\0';

    c = strchr(ss[s], '>');
    if(!c) return RE(-31, "segment tag not closed");
    c[0] = '\0';
    c++;
    if(entdec(c)) return RE(-32, "entity decode error");
    segments[s].msgid = c;

    //--- validate msgid --
    const char* msgidok = "._-@$&";
    while(*c)
    {
      if(!(isalnum(*c) || strchr(msgidok, *c))) return RE(-33, "invalid msgid char '%c' in '%s'", *c, segments[s].msgid);
      c++;
    }

    c = strstr(ss[s], " bytes=\"");
    if(!c) return RE(-34, "segment missing bytes attribute");
    c += 8; // length of ' bytes="'
    e = strchr(c, '"');
    if(!e) return RE(-35, "bytes attribute no closed");
    e[0] = '\0';
    size_t bytes = atol(c); // todo: num/overflow check?
    if(!bytes) return RE(-36, "invalid bytes attribute value");
    segments[s].bytes = bytes;
    e[0] = '"';

    c = strstr(ss[s], " number=\"");
    if(!c) return RE(-37, "segment missing number attribute");
    c += 9; // length of ' number="'
    e = strchr(c, '"');
    if(!e) return RE(-38, "number attribute no closed");
    e[0] = '\0';
    size_t number = atol(c); // todo: num/overflow check?
    if(!number) return RE(-39, "invalid number attribute value");
    segments[s].number = number;
    //e[0] = '"';
  }


  s = 0;
  for(f = 0; f < filec; f++)
  {
    files[f].segments = segments + s;
    while(s < segmentc && ss[s] < fe[f])
    {
      files[f].segmentc++;
      s++;
    }
    if(files[f].segmentc == 0) return RE(-41, "file with no segments, location %d", fs[f]-m_buf);
    qsort(files[f].segments, files[f].segmentc, sizeof(Segment), NXNzb::segsort);
    for(int sf = 0; sf < files[f].segmentc; sf++)
    {
      if(files[f].segments[sf].number != sf+1) return RE(-42, "segment %d missing, location %d", sf, files[f].segments[sf].msgid-m_buf);
    }
  }

  return 0;
}



/*static*/int NXNzb::segsort(const void* a, const void* b)
{
  NXNzb::Segment* ca = (NXNzb::Segment*)a;
  NXNzb::Segment* cb = (NXNzb::Segment*)b;
  return ca->number - cb->number;
}



int NXNzb::entdec(char* str)
{
  char* amp = str;
  char* end = str;
  while(*end) end++;

  while(amp = strchr(amp, '&'))
  {
    char* semi = strchr(amp+1, ';');
    if(!semi) return R(-1, "no entity end");

    //--- numeric entity ---
    if(amp[1] == '#')
    {
      int num = 0;
      if(amp[2] == 'x')
      {
        if(semi - amp > 5) return R(-11, "hex entity length exceeded");
        if(!isxdigit(amp[3])) return R(-12, "hex entity invalid 1");
        
             if(amp[3] >= 'A') num = amp[3] - 'A' + 10;
        else if(amp[3] >= 'a') num = amp[3] - 'a' + 10;
        else num = amp[3] - '0';
        
        if(semi - amp == 5)
        {
          num *= 16;
          if(!isxdigit(amp[4])) return R(-13, "hex entity invalid 2");

               if(amp[4] >= 'A') num += amp[4] - 'A' + 10;
          else if(amp[4] >= 'a') num += amp[4] - 'a' + 10;
          else num += amp[4] - '0';
        }
      }
      else
      {
        if(semi - amp > 5) return R(-14, "int entity length exceeded");
        char* s = amp+2;
        while(s < semi)
        {
          num *= 10;
          num += s[0] - '0';
          s++;
        }        
      }
      
      /*
      semi[0] = '\0';
      char* e;
      if(amp[2] == 'x')
        num = strtol(amp+3, &e, 16);
      else
        num = strtol(amp+2, &e, 10);
      */

      if(num < 32 || num > 255) return R(-15, "num entity out of range");
      amp[0] = num;
    }

    //--- named entity ---
    else
    {
           if(amp[1] == 'a' && amp[2] == 'm' && amp[3] == 'p') amp[0] = '&';
      else if(amp[1] == 'q' && amp[2] == 'u' && amp[3] == 'o' && amp[4] == 't') amp[0] = '"';
      else if(amp[1] == 'a' && amp[2] == 'p' && amp[3] == 'o' && amp[4] == 's') amp[0] = '\'';
      else if(amp[1] == 'l' && amp[2] == 't') amp[0] = '<';
      else if(amp[1] == 'g' && amp[2] == 't') amp[0] = '>';
      else return R(-21, "unsupported named entity %s", amp);
    }

    amp++;
    memcpy(amp, semi+1, end-semi);
  }

  return 0;
}



void NXNzb::cleanup()
{
  NXfree(m_buf); m_buf = NULL;
  filec = 0;
  NXfree(files); files = NULL;
  segmentc = 0;
  NXfree(segments); segments = NULL;
  NXfree(m_err); m_err = NXstrdup("no error");
}
