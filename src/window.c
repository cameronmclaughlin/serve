#include "window.h"

void PlaceChar(char c, int x, int y){
    printf("\033[%i;%iH%c", x, y, c);
}

void MoveCursor(int x, int y){
    printf("\033[%i;%iH", y + 1, x + 1);
}

int curRow = 0;
int logPos = 0;
char messageLog[128][128];

void LogMessage(char* input){
    LogMessageAlt(input, ProcessIndex[0].messageLog, &ProcessIndex[0].logPos);
    ProcessEntry* cur = &ProcessIndex[curProcess];
    if(curProcess != 0){
        LogMessageAlt(input, cur->messageLog, &cur->logPos);
    }
    return;
    snprintf(messageLog[logPos], sizeof(messageLog[logPos]), "%s", input);
    logPos++;

    if(logPos == ws.ws_row-1 || logPos == 127){
        logPos = 1;
    }

    WinRefresh();
}

void LogMessageAlt(char* input, char* log[512], int* logPos){
    snprintf(log[*logPos], 512, "%s", input);
    (*logPos)++;

    if(curRow == ws.ws_row-1 || *logPos == 511){
        *logPos = 1;
        curRow = 0;
    }

    WinRefresh();
}

#ifdef __linux__
unsigned long long get_total_cpu_ticks() {
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return 0;

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    char label[16];
    
    if (fscanf(fp, "%s %llu %llu %llu %llu %llu %llu %llu %llu", 
               label, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 9) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

unsigned long get_process_cpu_ticks(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;

    unsigned long utime = 0, stime = 0;
    if (fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu", 
               &utime, &stime) != 2) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return utime + stime;
}
#endif

void cpuTime(pid_t PID, uint64_t* user, uint64_t* system){
    #ifdef __APPLE__
        struct proc_taskinfo taskInfo;
        int ret = proc_pidinfo(PID, PROC_PIDTASKINFO, 0, &taskInfo, sizeof(taskInfo));
        if(ret == sizeof(taskInfo)){
            mach_timebase_info_data_t timebase;
            mach_timebase_info(&timebase);
            
            uint64_t user_nanoseconds = taskInfo.pti_total_user * timebase.numer / timebase.denom;
            uint64_t system_nanoseconds = taskInfo.pti_total_system * timebase.numer / timebase.denom;

            *user = user_nanoseconds / 1000;
            *system = system_nanoseconds / 1000;
        }
    #endif

    #ifdef __linux__
        *user = get_process_cpu_ticks(PID);
        *system = get_total_cpu_ticks();
    #endif
}

void PutChar(char c, int x, int y){
    MoveCursor(x, y);
    printf("%c", c);
}

void ClearSTDOUT(){
    // clear all stdout lines
    for(int i = 6; i < ws.ws_row-1; i++){
        MoveCursor(0, i);
        printf("\033[%i;%iH\033[K", i + 1, 1);
    }
}

void CustomPrint(char* str, int x, int y, int curIndex){
    int len = strlen(str);
    int curx = x;
    int cury = y;
    if(len == 0){ return; }
    ProcessEntry* curp = &ProcessIndex[curProcess];

    // print until \0, get final position of it
    for(int i = 0; i < len; i++){
        char cur = str[i];
        if(cur != '\r' && cur != '\t' && cur != '\033'){
            printf("\033[0m");
            PutChar(str[i], curx, cury);
            curx++;
        }else if(cur == '\033'){
            // get escape sequence in diff buffer. only accept m
            char* esc = (char*)malloc(len - i);
            memcpy(esc, &str[i], len - i);
            bool notSafeANSI = false;
            char* invalidANSI = "HfKABCD"; // make it safe for this
            int j = 0;
            for(j = 0; esc[j] != 'm' && j < len; j++){
                esc[j] = str[i+j];
                char c[2] = {esc[j], '\0'};
                if(strstr(invalidANSI, c) != NULL){
                    notSafeANSI = true;
                    break;
                }
            }
            if(!notSafeANSI){
                // copy ANSI into sep buffer
                char* ansiBuf = (char*)malloc(j);
                memset(ansiBuf, 0, j);
                memcpy(ansiBuf, (void*)&str[i], j-1);
                puts(ansiBuf);
                free(ansiBuf);
            }
            free(esc);
            i+=j;
        }
        if(curx == ws.ws_col || cur == '\n'){
            curx = 0;
            cury++;
        }
        if(cury == ws.ws_row-1){
            cury = 7;
            curRow = 0;
            // clear log behind cur and push
            ClearSTDOUT();
            for(int i = 0; i < curIndex; i++){
                memset(curp->messageLog[i], 0, 128);
            }
            memcpy(curp->messageLog[0], curp->messageLog[curIndex], 128);
            curp->logPos = 1;
            str = curp->messageLog[0];
        }
    }
    curRow += (cury - y);
}

void meminfo(unsigned long* total, unsigned long* available){
    FILE *fp = fopen("/proc/meminfo", "r");
    if(!fp){
        return;
    }

    char token[64];
    unsigned long value;
    int found = 0;

    // Parse the file line-by-line for key fields
    while (fscanf(fp, "%63s %lu %*s", token, &value) != EOF) {
        if (strcmp(token, "MemTotal:") == 0) {
            *total = value;
            found++;
        } else if (strcmp(token, "MemAvailable:") == 0) {
            *available = value;
            found++;
        }
        if (found == 2) break;
    }
    fclose(fp);
}

int lastLogPos = 0;
int lastSeenPID = 0;
double lastCpu = 0.0;

void WinRefresh(){
    ProcessEntry* cur = &ProcessIndex[curProcess];

    MoveCursor(0, 0);
    char curDir[PATH_MAX];
    getcwd(curDir, PATH_MAX);
    printf("\033[30;107m\033[KSERVE v%i.%i - %s (%i)\033[0m", VER_MAJ, VER_MIN, cur->Name, curProcess);

    double cpuPercent = 0.0;
    double physicalMem = 0.0;
    int threadNum = 0;

    #ifdef __APPLE__
        struct proc_taskinfo taskInfo;
        struct proc_bsdinfo bsdInfo;

        struct rusage_info_v4 rusage;

        if(cur->running){
            uint64_t u1, s1, u2, s2;
            cpuTime(cur->PID, &u1, &s1);
            usleep(10000);
            cpuTime(cur->PID, &u2, &s2);

            uint64_t deltaTime = (u2 - u1) + (s2 - s1);

            cpuPercent = ((double)deltaTime / 10000.0) * 100.0;

            int ret = proc_pidinfo(cur->PID, PROC_PIDTASKINFO, 0, &taskInfo, sizeof(taskInfo));
            if(ret == sizeof(taskInfo)){
                threadNum = taskInfo.pti_threadnum;
            }
            ret = proc_pidinfo(cur->PID, PROC_PIDTBSDINFO, 0, &bsdInfo, sizeof(bsdInfo));
            if(ret == sizeof(bsdInfo)){

            }
            ret = proc_pid_rusage(cur->PID, RUSAGE_INFO_V4, (rusage_info_t *)&rusage);
            if(ret != -1){
                physicalMem = (double)rusage.ri_phys_footprint / (double)1e6;
            }
        }
    #endif

    #ifdef __linux__
        // cpu and mem usage
        char path[64];
        snprintf(path, sizeof(path), "/proc/%i/statm", cur->PID);
        FILE* memFP = fopen(path, "r");
        if(memFP){
            unsigned long vmpages, rsspages;
            if (fscanf(memFP, "%lu %lu", &vmpages, &rsspages) == 2) {
                long page_size = sysconf(_SC_PAGESIZE); // Get page size in bytes
                unsigned long vmem_bytes = vmpages * page_size;
                unsigned long rss_bytes = rsspages * page_size;

                physicalMem = rss_bytes / 1024.0 / 1024.0; // Convert to MB
            }
            fclose(memFP);
        }

        // threads
        char* statusPath[PATH_MAX];
        snprintf(statusPath, PATH_MAX, "/proc/%i/status", cur->PID);
        FILE* fp = fopen(statusPath, "r");
        if(fp){
            char line[256];
            while(fgets(line, sizeof(line), fp)){
                if(strncmp(line, "Threads:", 8) == 0){
                    sscanf(line, "Threads:\t%d", &threadNum);
                    break;
                }
            }
            fclose(fp);
        }
    #endif

    bool innacurateCPU = cur->index == 0;

    MoveCursor(0, 1);
    printf("\033[30;47m\033[K%s", curDir);
    MoveCursor(0, 2);
    printf("\033[K\033[30;47mPID\033[0m %05i ", cur->PID);
    printf("\033[K\033[30;47mSTA\033[0m");
    switch(cur->state){
        case PROCESS_AVL:
            printf(" AVAILABLE");
            break;
        case PROCESS_RUNNING:
            printf(" RUNNING");
            break;
        case PROCESS_STOPPED:
            printf(" STOPPED %i", cur->exitCode);
            break;
        case PROCESS_SIGSTOP:
            printf(" SIGSTOP %i", cur->exitCode);
            break;
        case PROCESS_WAITING:
            printf(" WAITING");
            break;
    }
    MoveCursor(0, 3);
    printf("\033[K\033[30;47mCPU\033[0m %.2f%%", cpuPercent);
    printf(" \033[K\033[30;47mMEM\033[0m %.2fmb/%.2fgb (%.2f%%)", physicalMem, physicalMemSize, (physicalMem/(physicalMemSize*1000.0))*100.0);
    MoveCursor(0, 4);
    printf("\033[K\033[30;47mTHR\033[0m %05i", threadNum);
    printf(" \033[K\033[30;47mNET\033[0m");
    MoveCursor(0, 5);
    printf("\033[30;107m\033[KSTDOUT\033[0m");

    if(lastLogPos != cur->logPos || lastSeenPID != cur->PID || lastCpu != cpuPercent){
        ClearSTDOUT();

        // print the current process's message log
        curRow = 0;
        for(int i = 0; i < cur->logPos; i++){
            //MoveCursor(0, i+7);
            //printf("%s", cur->messageLog[i]);
            CustomPrint(cur->messageLog[i], 0, curRow+6, i);
        }

        lastLogPos = cur->logPos;
        lastSeenPID = cur->PID;
        lastCpu = cpuPercent;
    }

    MoveCursor(0, ws.ws_row);
    printf("\033[30;107m\033[K> %s", inputBuffer);
    MoveCursor(inputBufferPos+2, ws.ws_row);
    printf("\033[0m ");

    fflush(stdout);
}