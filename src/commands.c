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

bool searchPATH(char* name, char* finalPath){
    char* path = getenv("PATH");
    // get amount of path
    int count = 0;
    int len = strlen(path);
    for(int i = 0; i < len; i++){
        if(path[i] == ';' || path[i] == ':' || path[i] == '\0'){
            count++;
            path[i] = '\0';
        }
    }

    char* paths[count];
    int lastPos = 0;
    int seenPaths = 0;
    for(int i = 0; i < len; i++){
        if(path[i] == '\0'){
            paths[seenPaths++] = &path[lastPos];
            lastPos = i + 1;
        }
    }

    // now loop through each directory recursively
    for(int i = 0; i < seenPaths; i++){
        snprintf(finalPath, PATH_MAX, "%s/%s", paths[i], name);
        if(fileExists(finalPath)){
            return true;
        }
    }
    return false;
}

bool ProcessCommand(){
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
            return false;
        }
        chdir(fetch->cwd);
        SwitchProcess(fetch->index);
        snprintf(debug, 64, "Switched to %s (%i)", blocks[1].shortBlock, fetch->index);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "run", 3)){
        if(curProcess == 0){
            LogMessage("Cannot run on host");
            return false;
        }
        if(cur->running == true){
            LogMessage("Process already running");
            return false;
        }
        chdir(cur->cwd);
        char debug[128];
        char* args[blockCount];

        char path[PATH_MAX];
        memset(path, 0, PATH_MAX);
        // check if should resolve from path
        if(!fileExists(blocks[1].shortBlock)){
            if(!searchPATH(blocks[1].shortBlock, path)){
                LogMessage("No such file");
                return false;
            }
        }else{
            memcpy(path, blocks[1].shortBlock, strlen(blocks[1].shortBlock));
        }
        LogMessage(getenv("PATH"));

        for(int i = 0; i < blockCount - 1; i++){
            args[i] = blocks[i + 1].shortBlock;
        }
        args[blockCount-1] = NULL;
        RunProcess(&ProcessIndex[curProcess], path, blockCount - 1, args);
        snprintf(debug, 128, "Ran %s (%i)", path, curProcess);
        LogMessage(path);
        LogMessage(debug);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "kill", 4)){
        if(cur->running == false){
            LogMessage("Process already stopped");
            return false;
        }
        if(curProcess == 0){
            LogMessage("Cannot kill host process");
            return false;
        }
        char debug[64];
        snprintf(debug, 64, "Killed %s (%i)", cur->Name, curProcess);
        LogMessage(debug);
        KillProcess(curProcess);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "del", 3)){
        if(curProcess == 0){
            LogMessage("Cannot kill host entry");
            return false;
        }
        char debug[64];
        snprintf(debug, 64, "Killed entry %s (%i)", cur->Name, curProcess);
        LogMessage(debug);
        KillEntry(curProcess);
        chdir(ProcessIndex[0].cwd);
        SwitchProcess(0);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "cd", 2)){
        // change cur working directory
         if(blockCount < 2){
            LogMessage("No directory provided");
            return false;
        }
        if(curProcess == 0){
            LogMessage("Cannot change host CWD");
            return false;
        }
        char debug[128];
        int c = chdir(blocks[1].shortBlock);
        if(c != 0){
            LogMessage("CD: No such directory");
            return false;
        }
        char* newDir[PATH_MAX];
        getcwd(newDir, PATH_MAX);
        memcpy(cur->cwd, newDir, PATH_MAX);
        snprintf(debug, 128, "Changed cwd to %s", newDir);
        LogMessage(debug);
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
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "in", 2)){
        if(blockCount < 2){
            LogMessage("No input provided");
            return false;
        }
        // send input to process
        if(curProcess == 0){
            LogMessage("Cannot send input to host process");
            return false;
        }
        if(cur->running == false){
            LogMessage("Process not running");
            return false;
        }
        write(cur->STDINpipe[1], blocks[1].block, StringLength(blocks[1].block));
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "echo", 4)){
        if(blockCount < 2){
            LogMessage("No input provided");
            return false;
        }
        LogMessage(blocks[1].block);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "macro", 5)){
        if(blockCount < 2){
            LogMessage("No macro provided");
            return false;
        }
        char path[256];
        if(blocks[1].shortBlock[0] == 'p'){
            memcpy(path, blocks[2].shortBlock, strlen(blocks[2].shortBlock));
        }else{
            snprintf(path, 256, "macros/%s.macro", blocks[1].shortBlock);
            LogMessage(path);
        }

        struct stat fileStats; stat(path, &fileStats);
        char* macroBuffer = (char*)malloc(fileStats.st_size+1);
        memset(macroBuffer, 0, fileStats.st_size+1);
        int macroFD = open(path, O_RDONLY);
        if(macroFD < 0){
            LogMessage("Invalid macro name");
            return false;
        }
        int readS = read(macroFD, macroBuffer, fileStats.st_size);
        if(readS < 0){
            LogMessage("Failed to read macro");
            return false;
        }

        int lineCount = 0;
        for(int i = 0; i < fileStats.st_size+1; i++){
            if(macroBuffer[i] == '\n' || macroBuffer[i] == '\0'){
                lineCount++;
            }
        }

        char temp[128]; memcpy(temp, inputBuffer, 128);
        int tempPos = inputBufferPos;

        char* lines[64];
        int lastPos = 0;
        int seenLines = 0;
        for(int i = 0; i < fileStats.st_size+1; i++){
            // loop through each line until \n or \0
            if(macroBuffer[i] == '\n' || macroBuffer[i] == '\0'){
                macroBuffer[i] = '\0';
                lines[seenLines++] = &macroBuffer[lastPos];
                lastPos = i+1;
            }
        }

        // loop through every line and process it
        for(int i = 0; i < lineCount; i++){
            memcpy(inputBuffer, lines[i], 128);
            inputBufferPos = strlen(lines[i]);
            // ignore shebang
            if(inputBuffer[0] != '#'){
                bool p = ProcessCommand();
                if(!p){
                    LogMessage("Macro failed");
                    return false;
                }
            }
            //LogMessage(inputBuffer);
        }
        memcpy(inputBuffer, temp, 128);
        inputBufferPos = tempPos;

        close(macroFD);
        free(macroBuffer);
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "ls", 2)){
        #ifdef MAC
            DIR* dirfd = opendir(cur->cwd);
            if(dirfd == NULL){
                LogMessage("Invalid directory");
                return false;
            }
            struct dirent* entr;
            while((entr = readdir(dirfd)) != NULL){
                char colored[128];
                if(entr->d_type == DT_DIR){
                    snprintf(colored, 128, "[dir] %s", entr->d_name);
                }else if(entr->d_type == DT_FIFO){
                    snprintf(colored, 128, "[pipe] %s", entr->d_name);
                }else{
                    snprintf(colored, 128, "%s", entr->d_name);
                }
                LogMessage(colored);
            }
            closedir(dirfd);
        #endif
        #ifdef LINUX

        #endif
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "help", 4)){
        LogMessage("help - Show this");
        LogMessage("create <name> - Make a new process entry");
        LogMessage("switch <name> - Switch the current process");
        LogMessage("list - List all process entries");
        LogMessage("cd <dir> - Change the current working directory");
        LogMessage("run <dir> <args> - Run a process");
        LogMessage("in <input> - Send input to the current process");
        LogMessage("kill - Kill the current process");
        LogMessage("del - Delete the current process entry");
        LogMessage("clear - Clear the current screen");
        LogMessage("quit - Quit the manager, kill processes");
    }else if(CompareStrings(blocks[0].shortBlock, StringLength(blocks[0].shortBlock), "clear", 5)){
        cur->logPos = 0;
    }else{
        LogMessage("Invalid Command");
        return false;
    }
    memset(inputBuffer, 0, 128);

    inputBufferPos = 0;
    return true;
}