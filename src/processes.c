#include "processes.h"

ProcessEntry ProcessIndex[16];

int curProcess = 0;

void SwitchProcess(int index){
    curProcess = index;
}

bool CompareStrings(char* string0, size_t size0, char* string1, size_t size1){
    if(size0 != size1){
        return false;
    }
    for(int i = 0; i < size0; i++){
        if(string0[i] != string1[i]){
            return false;
        }
    }
    return true;
}

size_t StringLength(char* string){
    if(string == NULL){ return 0; }
    int i;
    for(i = 0; i < 256; i++){
        if(string[i] == '\0'){
            return i;
        }
    }
    return i;
}

ProcessEntry* FetchByPID(pid_t pid){
    for(int i = 0; i < 16; i++){
        ProcessEntry* cur = &ProcessIndex[i];
        if(cur->PID == pid){
            return cur;
        }
    }
    return NULL;
}

ProcessEntry* FetchByName(char* Name){
    // StringLength(foo) - 1 to get the len w/o \0
    size_t findLength = StringLength(Name)-1;
    for(int i = 0; i < 16; i++){
        ProcessEntry* cur = &ProcessIndex[i];
        if(cur->available == false){
            if(CompareStrings(cur->Name, StringLength(cur->Name)-1, Name, findLength)){
                cur->index = i;
                return cur;
            }
        }
    }
    return NULL;
}

void initProcesses(){
    memset(ProcessIndex, 0, sizeof(ProcessIndex));
    for(int i = 0; i < 16; i++){
        ProcessEntry* cur = &ProcessIndex[i];
        cur->available = true;
        cur->index = i;
    }

    ProcessEntry* hostEntry = NewProcess("host");
    hostEntry->PID = getpid();
    hostEntry->running = true;
    hostEntry->state = PROCESS_RUNNING;
}

ProcessEntry* FirstAvailable(){
    for(int i = 0; i < 16; i++){
        ProcessEntry* cur = &ProcessIndex[i];
        if(cur->available == true){
            cur->index = i;
            return cur;
        }
    }
    return NULL;
}

void KillEntry(int index){
    KillProcess(index);
    ProcessEntry* entry = &ProcessIndex[index];
    free(entry->buffer);
    for(int i = 0; i < 512; i++){
        free(entry->messageLog[i]);
    }
    free(entry->Name);
}

void KillProcess(int index){
    ProcessEntry* entry = &ProcessIndex[index];

    kill(entry->PID, SIGKILL);
    close(entry->pipeFD[0]);
    close(entry->pipeFD[1]);
}

void KillAllProcesses(){
    for(int i = 0; i < 16; i++){
        KillEntry(i);
    }
}

ProcessEntry* NewProcess(char* Name){
    ProcessEntry* avl = FirstAvailable();
    if(avl == NULL){
        return NULL;
    }
    int temp = avl->index;
    memset(avl, 0, sizeof(ProcessEntry));
    avl->index = temp;
    char* buffer = (char*)malloc(1024);
    avl->buffer = buffer;
    for(int i = 0; i < 128; i++){
        avl->messageLog[i] = (char*)malloc(512);
    }
    size_t len = StringLength(Name);
    avl->Name = (char*)malloc(len);
    avl->cwd = (char*)malloc(PATH_MAX);
    getcwd(avl->cwd, PATH_MAX);

    memcpy(avl->Name, Name, len);
    avl->available = false;
    return avl;
}

void GetProcessInput(ProcessEntry* entry){
    if(entry->running == false){
        return;
    }
    
    struct pollfd pfd;
    pfd.fd = entry->pipeFD[0];
    pfd.events = POLLIN;
    int lastTerm = 0;

    ssize_t bytesRead;
    if(poll(&pfd, 1, 0) >= 0 && pfd.revents & POLLIN){
        bytesRead = read(entry->pipeFD[0], entry->buffer, 1023);
        if(bytesRead > 0){
            entry->buffer[bytesRead] = '\0';
            LogMessageAlt(entry->buffer, entry->messageLog, &entry->logPos);
        }
    }
}

void RunProcess(ProcessEntry* entry, char* path, int argc, char* argv[]){
    char* buffer = entry->buffer;
    if(pipe(entry->pipeFD) == -1){
        KillProcess(entry->index);
        return;
    }

    pid_t PID = fork();
    entry->PID = PID;
    if(PID == 0){
        // CHILD
        close(entry->pipeFD[0]);

        if(dup2(entry->pipeFD[1], STDOUT_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        close(entry->pipeFD[1]);

        execve(path, argv, mainENVP);
        CleanUp();
        exit(EXIT_FAILURE);
    }

    entry->state = PROCESS_RUNNING;
    entry->running = true;
    close(entry->pipeFD[1]);
}