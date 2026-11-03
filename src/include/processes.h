#pragma once

#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"
#include "unistd.h"
#include "termios.h"
#include "stdlib.h"
#include <sys/ioctl.h>
#ifdef MAC
    #include <libproc.h>
#endif
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <limits.h>

typedef enum ProcessStatus{
    PROCESS_AVL,
    PROCESS_RUNNING,
    PROCESS_STOPPED,
    PROCESS_SIGSTOP,
    PROCESS_WAITING
} ProcessStatus;

typedef struct ProcessEntry{
    char* Name;
    pid_t PID;
    bool available;
    bool running;
    int index;
    int STDOUTpipe[2];
    int STDINpipe[2];
    char* buffer;
    char* messageLog[512];
    int logPos;
    int exitCode;
    char* cwd;
    ProcessStatus state;
} ProcessEntry;

extern ProcessEntry ProcessIndex[16];
extern int curProcess;

void initProcesses();

extern char** mainENVP;

bool CompareStrings(char* string0, size_t size0, char* string1, size_t size1);
size_t StringLength(char* string);
ProcessEntry* NewProcess(char* Name);
void KillProcess(int index);
void KillAllProcesses();
void KillEntry(int index);
void SwitchProcess(int index);
void RunProcess(ProcessEntry* entry, char* path, int argc, char* argv[]);
extern void LogMessage(char* input);
extern void LogMessageAlt(char* input, char* log[128], int* logPos);
extern void CleanUp();
ProcessEntry* FetchByName(char* Name);
void GetProcessInput(ProcessEntry* entry);
ProcessEntry* FetchByPID(pid_t pid);
bool fileExists(char* dir);