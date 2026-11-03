#pragma once

#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "unistd.h"
#include "termios.h"
#include "stdlib.h"
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef MAC
    #include <libproc.h>
    #include <mach/mach_time.h>
#endif

#include "processes.h"
#include "input.h"

#ifndef VER_MAJ
    #define VER_MAJ 0
#endif

#ifndef VER_MIN
    #define VER_MIN 0
#endif

extern int port;

extern int testPID;

extern double physicalMemSize;

void LogMessage(char* input);
void LogMessageAlt(char* input, char* log[128], int* logPos);
void WinRefresh();
void meminfo(unsigned long* total, unsigned long* available);