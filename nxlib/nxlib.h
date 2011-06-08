#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <share.h>
#include <direct.h>


#define _CRT_SECURE_NO_WARNINGS 1

const int NXLIBBUILD = 1;               // kind of a version number

const int NXNEWSHEAD = 512;             // newsfiles from nzbs will keep the "header" of rars for further parsing; since a nzb can have hundereds of rars the blocks might get pushed out of the memory and the requested again for rar parsing
const int NXBLOCKSIZE = 400000;         // 384000 is yenc "default" and some for overhead and magic stuff
const int NXMAXBLOCKSIZE = 4*1024*1024; // maximum the buffer can grow to
const int NXMAXMSGID = 64;
const int NXMAXCONN = 50;
const int NXRARPL = 3;                  // 10^# max rar part numbers
const int NXRARP = 1000;                // goes along with above
const int NXMHEAD = 75;
const int NXHBLOCKSIZE = 128*1024;        // http request blocksize/growths
const int NXMAXHBLOCKSIZE = 1*1024*1024;  // maximum the buffer can grow to
const int NXBZGROW = 20;                  // assumed de/compression ratio
const int NXGZGROW = 15;
const int NXYENCLINE = 128;


#include "message.h"
#include "auto.h"
#include "misc.h"
#include "nxlfs.h"
#include "confignews.h"
#include "nxnntp.h"
#include "nxnntppool.h"
#include "nxmutex.h"
#include "nxfile.h"
