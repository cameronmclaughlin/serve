#include "input.h"

char inputBuffer[128];
int inputBufferPos = 0;

volatile bool shouldProcessCommand = false;

char lastChar = 0;

volatile bool shouldRun = true;

char readRawKey(){
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1){
        return c;
    }
    return 0;
}

bool ProcessInput(){
    char rawKey = readRawKey();

    lastChar = rawKey;

    if (rawKey == '\x03'){
        // this is exit, CTRL+C
        shouldRun = false;
        return false;
    }else if(rawKey == 0){
        return false;
    }else if(rawKey >= ' ' && rawKey <= '~' && inputBufferPos < 128){
        inputBuffer[inputBufferPos++] = rawKey;
    }else if((rawKey == '\b' || rawKey == 0x7F) && inputBufferPos > 0){
        inputBufferPos--;
        inputBuffer[inputBufferPos] = 0;
    }else if(rawKey == '\r'){
        shouldProcessCommand = true;
    }

    return true;
}