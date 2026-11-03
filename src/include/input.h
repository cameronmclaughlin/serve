#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

extern volatile bool shouldRun;

extern char inputBuffer[128];
extern int inputBufferPos;

extern char lastChar;

extern volatile bool shouldProcessCommand;

bool ProcessInput();