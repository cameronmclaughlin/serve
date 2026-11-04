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
    entry->available = true;
    free(entry->buffer);
    for(int i = 0; i < 512; i++){
        free(entry->messageLog[i]);
    }
    free(entry->Name);
}

void KillProcess(int index){
    ProcessEntry* entry = &ProcessIndex[index];

    if(entry->running == true){
        kill(entry->PID, SIGKILL);
        close(entry->ptyMasterFD);
    }
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
    avl->Name = (char*)malloc(len+1);
    memset(avl->Name, 0, len+1);
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
    pfd.fd = entry->ptyMasterFD;
    pfd.events = POLLIN;

    ssize_t bytesRead;
    if(poll(&pfd, 1, 0) >= 0 && pfd.revents & POLLIN){
        bytesRead = read(entry->ptyMasterFD, entry->buffer, 1023);
        if(bytesRead > 0){
            entry->buffer[bytesRead] = '\0';
            LogMessageAlt(entry->buffer, entry->messageLog, &entry->logPos);
        }
    }
}

void RunProcess(ProcessEntry* entry, char* path, int argc, char* argv[]){
    char* buffer = entry->buffer;
    struct winsize miniTerm; miniTerm = ws; miniTerm.ws_row-=7;
    int pty = openpty(&entry->ptyMasterFD, &entry->ptySlaveFD, NULL, NULL, &miniTerm);
    if(pty < 0){ LogMessage("Failed to init pseudo-terminal"); return; }

    pid_t PID = fork();
    entry->PID = PID;
    if(PID == 0){
        // child process
        setsid();
        ioctl(entry->ptySlaveFD, TIOCSCTTY, 0);
        close(entry->ptyMasterFD);
        if(dup2(entry->ptySlaveFD, STDOUT_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        if(dup2(entry->ptySlaveFD, STDIN_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        if(dup2(entry->ptySlaveFD, STDERR_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }

        execve(path, argv, mainENVP);
        CleanUp();
        exit(EXIT_FAILURE);
    }

    close(entry->ptySlaveFD);

    int status = 0;
    waitpid(PID, &status, WNOHANG);
    if(WIFEXITED(status)){
        entry->running = false;
        entry->exitCode = WEXITSTATUS(status);
        entry->state = PROCESS_STOPPED;
    }else if(WIFSIGNALED(status)){
        entry->running = false;
        entry->exitCode = WTERMSIG(status);
        entry->state = PROCESS_SIGSTOP;
    }else if(WIFSTOPPED(status)){
        entry->state = PROCESS_WAITING;
        entry->exitCode = WSTOPSIG(status);
    }

    entry->state = PROCESS_RUNNING;
    entry->running = true;

    return;
    // old code below, replaced by pseudo terminal
    // stdout pipe
    /*
    if(pipe(entry->STDOUTpipe) == -1){
        KillProcess(entry->index);
        return;
    }
    // stdin pipe 
    if(pipe(entry->STDINpipe) == -1){
        KillProcess(entry->index);
        return;
    }
    // stderr pipe 
    if(pipe(entry->STDERRpipe) == -1){
        KillProcess(entry->index);
        return;
    }

    pid_t PID = fork();
    entry->PID = PID;
    if(PID == 0){
        // CHILD
        close(entry->STDOUTpipe[0]);
        close(entry->STDINpipe[1]);
        close(entry->STDERRpipe[0]);

        if(dup2(entry->STDOUTpipe[1], STDOUT_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        if(dup2(entry->STDINpipe[0], STDIN_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        if(dup2(entry->STDERRpipe[1], STDERR_FILENO) == -1){
            CleanUp();
            exit(EXIT_FAILURE);
        }
        close(entry->STDOUTpipe[1]);
        close(entry->STDINpipe[0]);
        close(entry->STDERRpipe[1]);

        execve(path, argv, mainENVP);
        CleanUp();
        exit(EXIT_FAILURE);
    }
    int status = 0;
    waitpid(PID, &status, WNOHANG);
    if(WIFEXITED(status)){
        entry->running = false;
        entry->exitCode = WEXITSTATUS(status);
        entry->state = PROCESS_STOPPED;
    }else if(WIFSIGNALED(status)){
        entry->running = false;
        entry->exitCode = WTERMSIG(status);
        entry->state = PROCESS_SIGSTOP;
    }else if(WIFSTOPPED(status)){
        entry->state = PROCESS_WAITING;
        entry->exitCode = WSTOPSIG(status);
    }

    entry->state = PROCESS_RUNNING;
    entry->running = true;
    close(entry->STDINpipe[0]);
    close(entry->STDERRpipe[1]);
    close(entry->STDOUTpipe[1]);
    */
}

bool fileExists(char* dir){
    return access(dir, F_OK) == 0;
}