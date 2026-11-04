#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <stdlib.h>
#ifdef __APPLE__
    #include <sys/sysctl.h>
    #include <mach/mach_time.h>
#endif


#include "window.h"
#include "input.h"
#include "commands.h"
#include "processes.h"

struct termios orig_termios;

char* buffer;

char nickname[32];
int nicknameLength = 0;

volatile sig_atomic_t forceExit = 0;

// clean up the terminal for exit
void CleanUp(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    write(STDOUT_FILENO, "\033[?1049l", 10); // switch to main framebuffer
    KillAllProcesses();
    signal(SIGWINCH, SIG_DFL);
}

// catch CTRL+C
void sigExitHandler(int dummy){
    forceExit = 1;
}

double physicalMemSize;

char** mainENVP;

struct winsize ws;

int main(int argc, char* argv[], char* envp[]){
    mainENVP = envp;
    // setup the ctrl+c catch
    struct sigaction act;
    act.sa_handler = sigExitHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("SIGINT registration failed");
        return EXIT_FAILURE;
    }

    // get system memory info
    int64_t avl = 0;
    int64_t memorySize = 0;
    size_t len = sizeof(memorySize);

    #ifdef __APPLE__
        int mib[2];
        mib[0] = CTL_HW;
        mib[1] = HW_MEMSIZE;
        if (sysctl(mib, 2, &memorySize, &len, NULL, 0) == 0) {
            physicalMemSize = memorySize / (1024 * 1024 * 1024);
        } else {
            perror("Error retrieving physical memory size");
            return EXIT_FAILURE;
        }
    #endif
    #ifdef __linux__
        unsigned long physMem;
        meminfo(&physMem, &avl);
        physicalMemSize = physMem / (1024 * 1024 * 1024);
    #endif

    // enable terminal raw mode
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // alternate framebuffer
    write(STDOUT_FILENO, "\033[?1049h", 9);
    write(STDOUT_FILENO, "\033[?25l", 7); // no cursor

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0){
        CleanUp();
        printf("Failed: couldn't get winsize");
        return EXIT_FAILURE;
    }
    signal(SIGWINCH, SIG_IGN); // ignore resize
    
    initProcesses();

    setbuf(stdout, NULL);
    WinRefresh();

    // after all this, check if it should execute a macro
    if(argc == 2){
        char macroCmd[128];
        int len = snprintf(macroCmd, 128, "macro p %s", argv[1]);
        inputBufferPos = len;
        memcpy(inputBuffer, macroCmd, len);
        ProcessCommand();
    }

    while(shouldRun && !forceExit){
        ProcessInput();

        WinRefresh();
        
        if(shouldProcessCommand){
            ProcessCommand();
        }

        int status;
        pid_t pid;

        // loop through each process and get input and status
        for(int i = 1; i < 16; i++){
            ProcessEntry* cur = &ProcessIndex[i];
            if(cur->available == false && cur->running == true){
                if((pid = waitpid(cur->PID, &status, WNOHANG | WUNTRACED)) <= 0){ GetProcessInput(cur); continue; }
                if(cur == NULL){
                    continue;
                }
                if(WIFEXITED(status)){
                    cur->running = false;
                    cur->exitCode = WEXITSTATUS(status);
                    cur->state = PROCESS_STOPPED;
                    continue;
                }else if(WIFSIGNALED(status)){
                    cur->running = false;
                    cur->exitCode = WTERMSIG(status);
                    cur->state = PROCESS_SIGSTOP;
                    continue;
                }else if(WIFSTOPPED(status)){
                    cur->state = PROCESS_WAITING;
                    cur->exitCode = WSTOPSIG(status);
                }
                GetProcessInput(cur);
            }
        }

        usleep(10000); // 10ms sleep
    }

    CleanUp();

    return 0;
}