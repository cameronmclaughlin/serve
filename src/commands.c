#include "commands.h"

int FirstSpace(char* str, int len){
    for(int i = 0; i < len; i++){
        if(str[i] == ' '){
            return i;
        }
    }

    return 0;
}

void SetNickname(char* nick, size_t nickLen){
    if(nickLen >= 32){
        return;
    }
    int file = open("data/nickname.txt", O_WRONLY | O_TRUNC);
    
    write(file, nick, nickLen);
}

void DumpContacts(){
    char* contactFile = "data/contacts.txt";

    LogMessage("CONTACTS LIST:");

    struct stat st;
    stat(contactFile, &st);

    char* fileBuffer = (char*)malloc(st.st_size);

    int contact_fd = open(contactFile, O_RDONLY);
    read(contact_fd, fileBuffer, st.st_size);

    char curContact[64];
    memset(curContact, 0, 64);
    int contactPos = 0;
    int i;
    for(i = 0; i < st.st_size; i++){
        if(fileBuffer[i] == '\n'){
            curContact[63] = 0;
            LogMessage(curContact);
            memset(curContact, 0, 64);
            contactPos = 0;
        }else if(contactPos >= 64){
            curContact[63] = 0;
            LogMessage(curContact);
            memset(curContact, 0, 64);
            contactPos = 0;
        }else{
            curContact[contactPos] = fileBuffer[i];
            contactPos++;
        }
    }

    curContact[63] = 0;
    LogMessage(curContact);

    free(fileBuffer);
    close(contact_fd);

    LogMessage("END OF CONTACTS");
}

int GetArgIndex(int arg){
    int spaces = 0;
    for(int i = 0; i < inputBufferPos; i++){
        if(inputBuffer[i] == ' '){
            spaces++;
        }
        if(spaces == arg){
            return i+1;
        }
    }
    return -1;
}

void ProcessCommand(){
    shouldProcessCommand = false;

    // practically, this just creates a list of BlockDesignators, each part of a command gets it's own.
    // space separated
    int blockCount = 1;

    char tempBuffer[128];
    memcpy(tempBuffer, inputBuffer, 128);

    for(int i = 0; i < inputBufferPos; i++){
        if(inputBuffer[i] == ' '){
            tempBuffer[i] = '\0';
            blockCount++;
        }
    }

    struct BlockDesignator* blocks = malloc(sizeof(struct BlockDesignator) * blockCount);
    memset(blocks, 0, sizeof(struct BlockDesignator) * blockCount);

    int lastBlockPos = 0;
    for(int i = 0; i < blockCount; i++){
        blocks[i].block = (char*)&inputBuffer[lastBlockPos];
        blocks[i].shortBlock = (char*)&tempBuffer[lastBlockPos];
        for(int c = lastBlockPos; c < inputBufferPos; c++){
            blocks[i].bytes = c-lastBlockPos;
            if(tempBuffer[c] == '\0'){
                break;
            }
        }
        lastBlockPos+= blocks[i].bytes+1;

        /*
        char debug[64];
        snprintf(debug, 64, "Size=%zu", blocks[i].bytes);
        LogMessage(debug);
        snprintf(debug, 64, "%s", blocks[i].block);
        LogMessage(debug);
        */
    }

    ProcessEntry* cur = &ProcessIndex[curProcess];

    if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "create", 6)){
        char debug[64];
        snprintf(debug, 64, "Created entry %s", blocks[1].shortBlock);
        LogMessage(debug);
        NewProcess(blocks[1].shortBlock);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "switch", 6)){
        char debug[64];
        ProcessEntry* fetch = FetchByName(blocks[1].shortBlock);
        if(fetch == NULL){
            snprintf(debug, 64, "Couldn't switch to %s: nonexistent", blocks[1].shortBlock);
            LogMessage(debug);
            return;
        }
        chdir(fetch->cwd);
        SwitchProcess(fetch->index);
        snprintf(debug, 64, "Switched to %s (%i)", blocks[1].shortBlock, fetch->index);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "run", 3)){
        if(cur->running == true){
            LogMessage("Process already running");
            return;
        }
        chdir(cur->cwd);
        char debug[64];
        char* args[blockCount];
        for(int i = 0; i < blockCount - 1; i++){
            args[i] = blocks[i + 1].shortBlock;
            LogMessage(args[i]);
        }
        args[blockCount-1] = NULL;
        RunProcess(&ProcessIndex[curProcess], blocks[1].shortBlock, blockCount - 1, args);
        snprintf(debug, 64, "Ran %s (%i)", blocks[1].shortBlock, curProcess);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "kill", 4)){
        if(cur->running == false){
            LogMessage("Process already stopped");
            return;
        }
        if(curProcess == 0){
            LogMessage("Cannot kill host process");
            return;
        }
        char debug[64];
        KillProcess(curProcess);
        snprintf(debug, 64, "Killed %s (%i)", cur->Name, curProcess);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "del", 3)){
        if(cur->available == false){
            LogMessage("Process already stopped");
            return;
        }
        if(curProcess == 0){
            LogMessage("Cannot kill host entry");
            return;
        }
        char debug[64];
        KillEntry(curProcess);
        chdir(ProcessIndex[0].cwd);
        SwitchProcess(0);
        snprintf(debug, 64, "Killed entry %s (%i)", cur->Name, curProcess);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "cwd", 3)){
        // change cur working directory
        if(curProcess == 0){
            LogMessage("Cannot change host CWD");
            return;
        }
        char debug[64];
        snprintf(debug, 64, "Changed cwd to %s", blocks[1].shortBlock);
        LogMessage(debug);
        memcpy(cur->cwd, blocks[1].shortBlock, PATH_MAX);
        chdir(cur->cwd);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "quit", 4)){
        LogMessage("Quitting...");
        CleanUp();
        exit(0);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "list", 4)){
        for(int i = 0; i < 16; i++){
            if(ProcessIndex[i].available == false){
                char debug[64];
                snprintf(debug, 64, "%s (%i)", ProcessIndex[i].Name, i);
                LogMessage(debug);
            }
        }
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "help", 4)){
        LogMessage("help - Show this");
        LogMessage("create <name> - Make a new process entry");
        LogMessage("switch <name> - Switch the current process");
        LogMessage("list - List all process entries");
        LogMessage("cwd <dir> - Change the currnet working directory");
        LogMessage("run <dir> <args> - Run a process");
        LogMessage("kill - Kill the current process");
        LogMessage("del - Delete the current process entry");
        LogMessage("clear - Clear the current screen");
        LogMessage("quit - Quit the manager, kill processes");
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "clear", 5)){
        cur->logPos = 0;
    }else{
        LogMessage("Invalid Command");
        return;
    }
    memset(inputBuffer, 0, 128);

    inputBufferPos = 0;
}