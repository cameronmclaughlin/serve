#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#ifdef __APPLE__
    #include <sys/types.h>
    #include <dirent.h>
#endif

#include "processes.h"
#include "input.h"
#include "window.h"

enum COMMAND_IDENT{
    CREATE,
    ANCHOR,
    IN,
    DELETE,
    KILL,
    LIST
};

struct Command{
    enum COMMAND_IDENT Ident;
    void* args[16];
    uint8_t argCount;
};

struct BlockDesignator{
    char* block;
    char* shortBlock;
    size_t bytes;
};

void SetNickname(char* nick, size_t nickLen);
bool ProcessCommand();