#include "nxlib.h"


#if(1 || defined _DEBUG)
int __stdcall RR(char** err, const char* fct, int r, const char* m,...) 
{ 

  va_list args;
  va_start(args, m);
  char msg[2048];
  int l = vsprintf_s(msg, 2048, m, args); 
  if(l == -1)
  {
    fprintf(stderr, "%s:%d\tlog buffer to small\n", fct, r); 
  }
  else
  {
    fprintf(stderr, "%s:%d\t%s\n", fct, r, msg); 
    if(err)
    {
      l++; // for the \0
      *err = (char*)NXrealloc(*err, l * sizeof(char));
      vsprintf_s(*err, l, m, args);
    }
  }
  va_end(args);
  return r; 
}
#endif



__time64_t NXtimet32to64(__time32_t t32)
{
  tm t;
  _localtime32_s(&t, &t32); // ERR
  __time64_t r = _mktime64(&t);
  if(r == -1) 
  {
    R(-1, "mktime error");
    return _time64(NULL);
  }
  return r;
}



 __time32_t NXtimet64to32(__time64_t t64)
{
  tm t;
  _localtime64_s(&t, &t64); // ERR
  __time32_t r = _mktime32(&t);
  if(r == -1) 
  {
    R(-1, "mktime error");
    return _time32(NULL);
  }
  return r;
}



__time64_t NXdostimeto64(__time32_t dostime)
{
  tm t;
  memset(&t, 0, sizeof(tm));
  t.tm_sec = 2 * (dostime & 0x1f);
  t.tm_min = (dostime >> 5) & 0x3f;
  t.tm_hour = (dostime >> 11) & 0x1f;
  t.tm_mday = (dostime >> 16) & 0x1f;
  t.tm_mon  = ((dostime >> 21) & 0x0f) - 1;
  t.tm_year = ((dostime >> 25) & 0x7f) + 1980 - 1900;

  __time64_t r = _mktime64(&t);
  if(r == -1) 
  {
    R(-1, "mktime error");
    return _time64(NULL);
  }
  return r;
}



__time32_t NXdostimeto32(__time32_t dostime)
{
  tm t;
  memset(&t, 0, sizeof(tm));
  t.tm_sec = 2 * (dostime & 0x1f);
  t.tm_min = (dostime >> 5) & 0x3f;
  t.tm_hour = (dostime >> 11) & 0x1f;
  t.tm_mday = (dostime >> 16) & 0x1f;
  t.tm_mon  = ((dostime >> 21) & 0x0f) - 1;
  t.tm_year = ((dostime >> 25) & 0x7f) + 1980 - 1900;


  __time32_t r = _mktime32(&t);
  if(r == -1) 
  {
    R(-1, "mktime error");
    return _time32(NULL);
  }
  return r;
}



#ifdef _DEBUG
wchar_t* NXstrdupw_dbg(const char* str, const char* file, int line)
{
  size_t len = strlen(str);
  wchar_t* r = (wchar_t*)_malloc_dbg(sizeof(wchar_t) * (len + 1), _NORMAL_BLOCK, file, line);
  mbstowcs_s(&len, r, len + 1, str, len); // ERR
  return r;
}


char* NXwcsdupc_dbg(const wchar_t* str, const char* file, int line)
{
  size_t len = wcslen(str);
  char* r = (char*)_malloc_dbg(sizeof(char) * (len + 1), _NORMAL_BLOCK, file, line);
  wcstombs_s(&len, r, len + 1, str, len); // ERR
  return r;
}

#else

wchar_t* NXstrdupw(const char* str)
{
  size_t len = strlen(str);
  wchar_t* r = (wchar_t*)NXmalloc(sizeof(wchar_t) * (len + 1));
  mbstowcs_s(&len, r, len + 1, str, len); // ERR
  return r;
}



char* NXwcsdupc(const wchar_t* str)
{
  size_t len = wcslen(str);
  char* r = (char*)NXmalloc(sizeof(char) * (len + 1));
  wcstombs_s(&len, r, len + 1, str, len); // ERR
  return r;
}
#endif



bool NXfextchk(const wchar_t* fn, const wchar_t* ext)
{
  return false;
}



void NXfextset(wchar_t* fn, const wchar_t* ext)
{
}



bool NXfextmchk(const char* fn, const char* ext)
{
  if(!ext || ext[0] == '\0') return false;

  size_t l = strlen(fn);
  const char* e = ext;
  for(;;)
  {
    const char* ne = strchr(e, '|');
    size_t el = ne ? ne-e : strlen(e);
    if(el < l && _strnicmp(fn+l-el, e, el) == 0) return true;
    if(!ne) break;
    e = ne+1;
  }
  return false;
}



bool NXfnammchk(const char* fn, const char* nam)
{
  if(!nam || nam[0] == '\0') return false;

  char* fna = _strlwr(strdupa(fn));
  char* nama = _strlwr(strdupa(nam));

  size_t l = strlen(fn);
  char* e = nama;
  for(;;)
  {
    char* ne = strchr(e, '|');
    if(ne) ne[0] = '\0';
    if(strstr(fna, e)) return true;
    if(!ne) break;
    e = ne+1;
  }
  return false;
}



#ifdef _DEBUG
#define nNoMansLandSize 4

typedef struct _CrtMemBlockHeader
{
        struct _CrtMemBlockHeader * pBlockHeaderNext;
        struct _CrtMemBlockHeader * pBlockHeaderPrev;
        unsigned char *                      szFileName;
        int                         nLine;
#ifdef _WIN64
        /* These items are reversed on Win64 to eliminate gaps in the struct
         * and ensure that sizeof(struct)%16 == 0, so 16-byte alignment is
         * maintained in the debug heap.
         */
        int                         nBlockUse;
        size_t                      nDataSize;
#else  /* _WIN64 */
        size_t                      nDataSize;
        int                         nBlockUse;
#endif  /* _WIN64 */
        long                        lRequest;
        unsigned char               gap[nNoMansLandSize];
        /* followed by:
         *  unsigned char           data[nDataSize];
         *  unsigned char           anotherGap[nNoMansLandSize];
         */
} _CrtMemBlockHeader;


class NXMemLog
{
public:
  NXMemLog() 
  {
    m_fh = _fsopen("mem.log", "w+", SH_DENYNO);
    _CrtSetAllocHook(NXMemLog::allochook);
  }
  ~NXMemLog()
  {
    fclose(m_fh);
  }

private:
  static int __cdecl allochook(int nAllocType, void* pvData, size_t nSize, int nBlockUse, long lRequest, const unsigned char* szFileName, int nLine)
  {
    char* operation[] = { "", "alloc", "realloc", "free" };

    if(nBlockUse == _CRT_BLOCK)
      return 1;
    
    if(nAllocType == 3 && pvData)
    {
      _CrtMemBlockHeader* bh = (_CrtMemBlockHeader*)((char*)pvData-sizeof(_CrtMemBlockHeader));
      lRequest = bh->lRequest;
      szFileName = bh->szFileName;
      nLine = bh->nLine;
      nSize = bh->nDataSize;
    }
    
    fprintf(m_fh, "%s\t%d\t%ld\t%p\t%s\t%d\n", operation[nAllocType], nSize, lRequest, pvData, szFileName, nLine);

    return 1;
  }

  static FILE* m_fh;
};
FILE* NXMemLog::m_fh = NULL;
NXMemLog NXMEMLOG;
#endif



#define DOS_STAR                        (L'<')
#define DOS_QM                          (L'>')
#define DOS_DOT                         (L'"')

// check whether Name matches Expression
// Expression can contain "?"(any one character) and "*" (any string)
// when IgnoreCase is TRUE, do case insenstive matching
//
// http://msdn.microsoft.com/en-us/library/ff546850(v=VS.85).aspx
// * (asterisk) Matches zero or more characters.
// ? (question mark) Matches a single character.
// DOS_DOT Matches either a period or zero characters beyond the name string.
// DOS_QM Matches any single character or, upon encountering a period or end
//        of name string, advances the expression to the end of the set of
//        contiguous DOS_QMs.
// DOS_STAR Matches zero or more characters until encountering and matching
//          the final . in the name.
BOOL NXIsNameInExpression(
	LPCWSTR		Expression, // matching pattern
	LPCWSTR		Name, // file name
	BOOL		IgnoreCase)
{
	ULONG ei = 0;
	ULONG ni = 0;

	while (Expression[ei] != '\0') {

		if (Expression[ei] == L'*') {
			ei++;
			if (Expression[ei] == '\0')
				return TRUE;

			while (Name[ni] != '\0') {
				if (NXIsNameInExpression(&Expression[ei], &Name[ni], IgnoreCase))
					return TRUE;
				ni++;
			}

		} else if (Expression[ei] == DOS_STAR) {

			ULONG p = ni;
			ULONG lastDot = 0;
			ei++;
			
			while (Name[p] != '\0') {
				if (Name[p] == L'.')
					lastDot = p;
				p++;
			}
			

			while (TRUE) {
				if (Name[ni] == '\0' || ni == lastDot)
					break;

				if (NXIsNameInExpression(&Expression[ei], &Name[ni], IgnoreCase))
					return TRUE;
				ni++;
			}
			
		} else if (Expression[ei] == DOS_QM)  {
			
			ei++;
			if (Name[ni] != L'.') {
				ni++;
			} else {

				ULONG p = ni + 1;
				while (Name[p] != '\0') {
					if (Name[p] == L'.')
						break;
					p++;
				}

				if (Name[p] == L'.')
					ni++;
			}

		} else if (Expression[ei] == DOS_DOT) {
			ei++;

			if (Name[ni] == L'.')
				ni++;

		} else {
			if (Expression[ei] == L'?') {
				ei++; ni++;
			} else if(IgnoreCase && towupper(Expression[ei]) == towupper(Name[ni])) {
				ei++; ni++;
			} else if(!IgnoreCase && Expression[ei] == Name[ni]) {
				ei++; ni++;
			} else {
				return FALSE;
			}
		}
	}

	if (ei == wcslen(Expression) && ni == wcslen(Name))
		return TRUE;
	

	return FALSE;
}




bool NXIsNameInExpression(
	const char*		Expression, // matching pattern
	const char*		Name, // file name
	bool		IgnoreCase)
{
	ULONG ei = 0;
	ULONG ni = 0;

	while (Expression[ei] != '\0') {

		if (Expression[ei] == '*') {
			ei++;
			if (Expression[ei] == '\0')
				return TRUE;

			while (Name[ni] != '\0') {
				if (NXIsNameInExpression(&Expression[ei], &Name[ni], IgnoreCase))
					return TRUE;
				ni++;
			}

		} else if (Expression[ei] == DOS_STAR) {

			ULONG p = ni;
			ULONG lastDot = 0;
			ei++;
			
			while (Name[p] != '\0') {
				if (Name[p] == '.')
					lastDot = p;
				p++;
			}
			

			while (TRUE) {
				if (Name[ni] == '\0' || ni == lastDot)
					break;

				if (NXIsNameInExpression(&Expression[ei], &Name[ni], IgnoreCase))
					return TRUE;
				ni++;
			}
			
		} else if (Expression[ei] == DOS_QM)  {
			
			ei++;
			if (Name[ni] != '.') {
				ni++;
			} else {

				ULONG p = ni + 1;
				while (Name[p] != '\0') {
					if (Name[p] == '.')
						break;
					p++;
				}

				if (Name[p] == '.')
					ni++;
			}

		} else if (Expression[ei] == '"') {
			ei++;

			if (Name[ni] == '.')
				ni++;

		} else {
			if (Expression[ei] == '?') {
				ei++; ni++;
			} else if(IgnoreCase && toupper(Expression[ei]) == toupper(Name[ni])) {
				ei++; ni++;
			} else if(!IgnoreCase && Expression[ei] == Name[ni]) {
				ei++; ni++;
			} else {
				return FALSE;
			}
		}
	}

	if (ei == strlen(Expression) && ni == strlen(Name))
		return TRUE;
	

	return FALSE;
}








NXLog NXLOG =
{
/*get*/   0,
/*head*/  0,
/*dupe*/  0,
/*cache*/ 0,
/*mutex*/ 0,
/*thread*/0,
/*perf*/  0,
};

