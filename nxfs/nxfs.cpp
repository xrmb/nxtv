#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "dokan.h"
#include "fileinfo.h"

//#include <vld.h>

#include "nxlfs.h"



NXLFS* g_nxlfs = NULL;

BOOL g_UseStdErr;
BOOL g_DebugMode;

static WCHAR MountPoint[MAX_PATH] = L"M:";

unsigned long __stdcall killerthread(void* nxthread)
{
  NXThread<int>* t = reinterpret_cast<NXThread<int>*>(nxthread);
  if(!t) { return 1; }

  for(;;)
  {
    if(_kbhit())
    {
      int c = _getch();
      switch(c)
      {
      case 'x':
        {
          NXNNTPPool* pool = NXNNTPPool::i();
          if(pool) pool->shutdown();
          //if(DokanUnmount(MountPoint[0]))
          if(DokanRemoveMountPoint(MountPoint))
            return 1;
          fprintf(stderr, "unmount failed?\n");
        }
        break;

      case 'h':
        NXLOG.head = NXLOG.head ? 0 : 1;
        break;

      case 'g':
        NXLOG.get = NXLOG.get ? 0 : 1;
        break;

      case 't':
        NXLOG.thread = NXLOG.thread ? 0 : 1;
        break;

      case 'm':
        NXLOG.mutex = NXLOG.mutex ? 0 : 1;
        break;

      case 'c':
        NXLOG.cache = NXLOG.cache ? 0 : 1;
        break;

      case 'p':
        NXLOG.perf = NXLOG.perf ? 0 : 1;
        break;

      default:
        printf("x ... exit\n"
               "h ... log headers\n"
               "g ... log get messages\n"
               "t ... log threads\n"
               "m ... log mutexes\n"
               "c ... log cache\n"
               );
      }
    }
    if(t->terminated())
      break;
    Sleep(500);
  }

  return 0;
}



FILETIME time64_t2FILETIME(__time64_t time)
{
  tm t;
  _localtime64_s(&t, &time);

  SYSTEMTIME s;
  memset(&s, 0, sizeof(SYSTEMTIME));
  s.wYear = t.tm_year + 1900;
  s.wMonth = t.tm_mon + 1;
  s.wDay = t.tm_mday;
  s.wHour = t.tm_hour;
  s.wMinute = t.tm_min;
  s.wSecond = t.tm_sec;

  FILETIME r;
  SystemTimeToFileTime(&s, &r);

  return r;
}

static void DbgPrint(LPCWSTR format, ...)
{
  if (g_DebugMode) {
    WCHAR buffer[512];
    va_list argp;
    va_start(argp, format);
    vswprintf_s(buffer, sizeof(buffer)/sizeof(WCHAR), format, argp);
    va_end(argp);
    if (g_UseStdErr) {
      fwprintf(stderr, buffer);
    } else {
      OutputDebugStringW(buffer);
    }
  }
}


static void GetFilePath(PWCHAR filePath, ULONG numberOfElements, LPCWSTR FileName)
{
  RtlZeroMemory(filePath, numberOfElements * sizeof(WCHAR));
  wcsncat_s(filePath, numberOfElements, FileName, wcslen(FileName));
}

#define NXLFSCheckFlag(val, flag) if (val&flag) { DbgPrint(L"\t" L#flag L"\n"); }



static int NXLFSCreateFile(LPCWSTR FileName, DWORD AccessMode, DWORD ShareMode, DWORD CreationDisposition, DWORD FlagsAndAttributes, PDOKAN_FILE_INFO DokanFileInfo)
{
	if(CreationDisposition != OPEN_EXISTING) return -1;
/*
	DbgPrint(L"\tAccessMode = 0x%x\n", AccessMode);

	NXLFSCheckFlag(AccessMode, GENERIC_READ);
	NXLFSCheckFlag(AccessMode, GENERIC_WRITE);
	NXLFSCheckFlag(AccessMode, GENERIC_EXECUTE);
	
	NXLFSCheckFlag(AccessMode, DELETE);
	NXLFSCheckFlag(AccessMode, FILE_READ_DATA);
	NXLFSCheckFlag(AccessMode, FILE_READ_ATTRIBUTES);
	NXLFSCheckFlag(AccessMode, FILE_READ_EA);
	NXLFSCheckFlag(AccessMode, READ_CONTROL);
	NXLFSCheckFlag(AccessMode, FILE_WRITE_DATA);
	NXLFSCheckFlag(AccessMode, FILE_WRITE_ATTRIBUTES);
	NXLFSCheckFlag(AccessMode, FILE_WRITE_EA);
	NXLFSCheckFlag(AccessMode, FILE_APPEND_DATA);
	NXLFSCheckFlag(AccessMode, WRITE_DAC);
	NXLFSCheckFlag(AccessMode, WRITE_OWNER);
	NXLFSCheckFlag(AccessMode, SYNCHRONIZE);
	NXLFSCheckFlag(AccessMode, FILE_EXECUTE);
	NXLFSCheckFlag(AccessMode, STANDARD_RIGHTS_READ);
	NXLFSCheckFlag(AccessMode, STANDARD_RIGHTS_WRITE);
	NXLFSCheckFlag(AccessMode, STANDARD_RIGHTS_EXECUTE);
  if(AccessMode != GENERIC_READ) return -2;
*/
  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  NXLFSFile* ff = const_cast<NXLFSFile*>(r->findfile(FileName));
  if(!ff) return -2;

  return ff->open();
}


static int NXLFSCreateDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  return -1;
}
static int NXLFSOpenDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  //--- we could call a folders build function ---
  return 0;
}
static int NXLFSCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  //--- cleanup did everything ---
  return 0;
}


static int NXLFSCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  NXLFSFile* ff = const_cast<NXLFSFile*>(r->findfile(FileName));
  if(!ff) return -2;

  return ff->close();
}


static int NXLFSReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo)
{
  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  NXLFSFile* ff = const_cast<NXLFSFile*>(r->findfile(FileName));
  if(!ff) return -2;

  size_t rl = BufferLength;
  int ret = ff->read(Buffer, BufferLength, &rl, Offset, 1); // todo: readahead per filetype/process?
  *ReadLength = rl;

  return ret;
}


static int NXLFSWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo)
{
  return -1;
}
static int NXLFSFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}


static int NXLFSGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION HandleFileInformation, PDOKAN_FILE_INFO DokanFileInfo)
{
  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  const NXLFSFile* ff = r->findfile(FileName);
  if(!ff) return -2;

  HandleFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
  HandleFileInformation->ftCreationTime =
  HandleFileInformation->ftLastAccessTime =
  HandleFileInformation->ftLastWriteTime = time64_t2FILETIME(ff->time());
  HandleFileInformation->dwVolumeSerialNumber = 0;
  HandleFileInformation->nFileSizeHigh = (DWORD)(ff->size() >> 32);
  HandleFileInformation->nFileSizeLow = (DWORD)ff->size();
  HandleFileInformation->nNumberOfLinks = 0;
  HandleFileInformation->nFileIndexHigh = 0;
  HandleFileInformation->nFileIndexLow = 0;

  return 0;
}


static int NXLFSFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo)
{
  WIN32_FIND_DATAW  findData;

  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  r = r->findfolder(FileName);
  if(!r) return -2;

  for(const NXLFSFolder* fd = r->folders(); fd; fd = fd->next())
  {
    wcscpy_s(findData.cFileName, MAX_PATH, fd->name());
    findData.nFileSizeLow = 0;
    findData.nFileSizeHigh = 0;
    findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    findData.ftCreationTime = 
    findData.ftLastAccessTime = 
    findData.ftLastWriteTime = time64_t2FILETIME(fd->time());
    FillFindData(&findData, DokanFileInfo);
  }

  for(const NXLFSFile* ff = r->files(); ff; ff = ff->next())
  {
    wcscpy_s(findData.cFileName, MAX_PATH, ff->name());
    findData.nFileSizeLow = (DWORD)ff->size();
    findData.nFileSizeHigh = (DWORD)(ff->size() >> 32);
    findData.ftCreationTime = findData.ftLastAccessTime = findData.ftLastWriteTime = time64_t2FILETIME(ff->time());
    findData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    FillFindData(&findData, DokanFileInfo);
  }

  return 0;
}


static int NXLFSFindFilesWithPattern(LPCWSTR FileName, LPCWSTR SearchPattern, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo)
{
  WIN32_FIND_DATAW  findData;

  const NXLFSFolder* r = g_nxlfs->root();
  if(!r) return -1;

  r = r->findfolder(FileName);
  if(!r) return -2;

  for(const NXLFSFolder* fd = r->folders(); fd; fd = fd->next())
  {
    if(!DokanIsNameInExpression(SearchPattern, fd->name(), true)) continue;

    wcscpy_s(findData.cFileName, MAX_PATH, fd->name());
    findData.nFileSizeLow = 0;
    findData.nFileSizeHigh = 0;
    findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    findData.ftCreationTime = 
    findData.ftLastAccessTime = 
    findData.ftLastWriteTime = time64_t2FILETIME(fd->time());
    FillFindData(&findData, DokanFileInfo);
  }

  for(const NXLFSFile* ff = r->files(); ff; ff = ff->next())
  {
    if(!DokanIsNameInExpression(SearchPattern, ff->name(), true)) continue;

    wcscpy_s(findData.cFileName, MAX_PATH, ff->name());
    findData.nFileSizeLow = (DWORD)ff->size();
    findData.nFileSizeHigh = (DWORD)(ff->size() >> 32);
    findData.ftCreationTime = findData.ftLastAccessTime = findData.ftLastWriteTime = time64_t2FILETIME(ff->time());
    findData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    FillFindData(&findData, DokanFileInfo);
  }

  return 0;
}


static int NXLFSDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  return -1;
}
static int NXLFSDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
  return -1;
}
static int NXLFSMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo)
{
  return -1;
}
static int NXLFSLockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSSetFileTime(LPCWSTR FileName, CONST FILETIME* CreationTime, CONST FILETIME* LastAccessTime, CONST FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSUnlockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}
static int NXLFSSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorLength, PDOKAN_FILE_INFO DokanFileInfo)
{
  return 0;
}

static int NXLFSGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO)
{
  *FreeBytesAvailable = *TotalNumberOfBytes = *TotalNumberOfFreeBytes = g_nxlfs->size();
  return 0;
}
 
static int NXLFSGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo)
{
  wcscpy_s(VolumeNameBuffer, VolumeNameSize / sizeof(WCHAR), L"NXFS");
  *VolumeSerialNumber = 0;
  *MaximumComponentLength = 256;
  *FileSystemFlags = 
    //FILE_CASE_SENSITIVE_SEARCH | 
    FILE_CASE_PRESERVED_NAMES | 
    //FILE_SUPPORTS_REMOTE_STORAGE |
    FILE_UNICODE_ON_DISK |
    FILE_READ_ONLY_VOLUME
    //FILE_PERSISTENT_ACLS
    ;

  wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), L"NXFS");

  return 0;
}


static int NXLFSUnmount(PDOKAN_FILE_INFO DokanFileInfo) 
{ 
  fprintf(stderr, "unount\n");
  return 0;
}


int __cdecl wmain(ULONG argc, wchar_t* argv[])
{
  int status;
  ULONG command;
  PDOKAN_OPERATIONS dokanOperations =
      (PDOKAN_OPERATIONS)malloc(sizeof(DOKAN_OPERATIONS));
  PDOKAN_OPTIONS dokanOptions =
      (PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));

  g_DebugMode = FALSE;
  g_UseStdErr = FALSE;

  wchar_t* root = _wcsdup(L"root");

  ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));
  dokanOptions->Version = DOKAN_VERSION;
  dokanOptions->ThreadCount = 5; // 0 = use default
  dokanOptions->MountPoint = MountPoint;

#ifdef _DEBUG
  NXLOG.get = 0;
  NXLOG.head = 0;
  NXLOG.perf = 0;
  NXLOG.dupe = 0;
#endif

  for (command = 1; command < argc; command++) {
    switch (towlower(argv[command][1])) {
    case L'h':
      fprintf(stderr, "nxfs.exe\n"
        "  /l <drive> (ex. /l m), defaults to m\n"
        "  /r <root> (ex. /l c:\\nxfsroot), defaults to .\\root\n"
        //"  /t ThreadCount (ex. /t 5)\n"
        //"  /d (enable debug output)\n"
        //"  /s (use stderr for output)\n"
      );
      return 0;
      break;
    case L'l':
      command++;
      if(command == argc)
      {
        fprintf(stderr, "incomplete command: %ws\n", argv[command-1]);
        return -1;
      }
      wcscpy_s(MountPoint, sizeof(MountPoint)/sizeof(WCHAR), argv[command]);
      break;
    case L't':
      command++;
      if(command == argc)
      {
        fprintf(stderr, "incomplete command: %ws\n", argv[command-1]);
        return -1;
      }
      dokanOptions->ThreadCount = (USHORT)_wtoi(argv[command]);
      break;
    case L'd':
      g_DebugMode = TRUE;
      break;
    case L's':
      g_UseStdErr = TRUE;
      break;
    case L'c':
      NXLFSFolder::collections(false);
      break;
    case L'r':
      command++;
      if(command == argc)
      {
        fprintf(stderr, "incomplete command: %ws\n", argv[command-1]);
        return -1;
      }
      free(root);
      root = _wcsdup(argv[command]);
      break;
    default:
      fprintf(stderr, "unknown command: %ws\n", argv[command]);
      return -1;
    }
  }

  if(_waccess(root, 0x04))
  {
    fprintf(stderr, "cant read root folder %ws", root);
    return -2;
  }

  if (g_DebugMode) {
    //dokanOptions->Options |= DOKAN_OPTION_DEBUG;
  }
  if (g_UseStdErr) {
    //dokanOptions->Options |= DOKAN_OPTION_STDERR;
  }

  dokanOptions->Options |= DOKAN_OPTION_KEEP_ALIVE;
  dokanOptions->Options |= DOKAN_OPTION_REMOVABLE;

  ZeroMemory(dokanOperations, sizeof(DOKAN_OPERATIONS));
  dokanOperations->CreateFile = NXLFSCreateFile;
  dokanOperations->OpenDirectory = NXLFSOpenDirectory;
  dokanOperations->CreateDirectory = NXLFSCreateDirectory;
  dokanOperations->Cleanup = NXLFSCleanup;
  dokanOperations->CloseFile = NXLFSCloseFile;
  dokanOperations->ReadFile = NXLFSReadFile;
  dokanOperations->WriteFile = NXLFSWriteFile;
  dokanOperations->FlushFileBuffers = NXLFSFlushFileBuffers;
  dokanOperations->GetFileInformation = NXLFSGetFileInformation;
  //dokanOperations->FindFiles = NXLFSFindFiles;
  dokanOperations->FindFilesWithPattern = NXLFSFindFilesWithPattern;
  dokanOperations->SetFileAttributes = NXLFSSetFileAttributes;
  dokanOperations->SetFileTime = NXLFSSetFileTime;
  dokanOperations->DeleteFile = NXLFSDeleteFile;
  dokanOperations->DeleteDirectory = NXLFSDeleteDirectory;
  dokanOperations->MoveFile = NXLFSMoveFile;
  dokanOperations->SetEndOfFile = NXLFSSetEndOfFile;
  dokanOperations->SetAllocationSize = NXLFSSetAllocationSize;  
  dokanOperations->LockFile = NXLFSLockFile;
  dokanOperations->UnlockFile = NXLFSUnlockFile;
  dokanOperations->GetFileSecurity = NXLFSGetFileSecurity;
  dokanOperations->SetFileSecurity = NXLFSSetFileSecurity;
  dokanOperations->GetDiskFreeSpace = NXLFSGetDiskFreeSpace;
  dokanOperations->GetVolumeInformation = NXLFSGetVolumeInformation;
  dokanOperations->Unmount = NXLFSUnmount;

  NXLFSConfigNews cfgn;
  if(cfgn.init(argv[0]))
  {
    fprintf(stderr, "init config error\n");
    return __LINE__;
  }

  NXLFSMessageCache mcache(cfgn.cache(), cfgn.maxdisk(), cfgn.maxmem());
  if(mcache.init())
  {
    fprintf(stderr, "init cache error\n");
    return __LINE__;
  }

  NXNNTPPool nxnntppool;
  if(nxnntppool.init(cfgn.conn())) 
  {
    fprintf(stderr, "init connection pool error\n");
    return __LINE__;
  }

  NXLFS nxlfs;
  if(nxlfs.init(root)) 
  {
    fprintf(stderr, "init file system error\n");
    return __LINE__;
  }

  g_nxlfs = &nxlfs;

  NXThread<int>* killer = new NXThread<int>(killerthread, NULL);



  status = DokanMain(dokanOptions, dokanOperations);
  switch (status) {
  case DOKAN_SUCCESS:
    fprintf(stderr, "Success\n");
    break;
  case DOKAN_ERROR:
    fprintf(stderr, "Error\n");
    break;
  case DOKAN_DRIVE_LETTER_ERROR:
    fprintf(stderr, "Bad Drive letter\n");
    break;
  case DOKAN_DRIVER_INSTALL_ERROR:
    fprintf(stderr, "Can't install driver\n");
    break;
  case DOKAN_START_ERROR:
    fprintf(stderr, "Driver something wrong\n");
    break;
  case DOKAN_MOUNT_ERROR:
    fprintf(stderr, "Can't assign a drive letter\n");
    break;
  case DOKAN_MOUNT_POINT_ERROR:
    fprintf(stderr, "Mount point error\n");
    break;
  default:
    fprintf(stderr, "Unknown error: %d\n", status);
    break;
  }

  free(dokanOptions);
  free(dokanOperations);

  g_nxlfs = NULL;
  free(root); root = NULL;
  delete killer; killer = NULL;

  return status;
}

