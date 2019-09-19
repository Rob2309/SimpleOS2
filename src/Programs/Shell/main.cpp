#include <simpleos_vfs.h>
#include <simpleos_process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char g_CmdBuffer[128] = { 0 };
static int g_CmdBufferIndex = 0;

static void HandleBackspace() {
    if(g_CmdBufferIndex > 0) {
        g_CmdBufferIndex--;
        g_CmdBuffer[g_CmdBufferIndex] = '\0';
        puts("\b");
    }
}

static char* GetToken(char*& buf) {
    while(*buf == ' ')
        buf++;

    if(*buf == '\0')
        return nullptr;

    char* res = buf;

    while(true) {
        if(*buf == ' ') {
            *buf = '\0';
            buf++;
            return res;
        }
        if(*buf == '\0') {
            return res;
        }

        buf++;
    }
}

static void InvokeCommand() {
    int argc = 0;
    char* argv[100];

    char* buf = g_CmdBuffer;
    char* arg;
    while((arg = GetToken(buf)) != nullptr) {
        argv[argc] = arg;
        argc++;
    }

    if(argc == 0)
        return;

    exec(argv[0], argc, argv);

    puts("Command not found\n");
}

static void HandleCommand() {
    puts("\n");

    if(g_CmdBufferIndex == 0)
        return;

    if(strcmp(g_CmdBuffer, "exit") == 0) {
        exit(0);
    } else if(strcmp(g_CmdBuffer, "whoami") == 0) {
        char buffer[128];
        whoami(buffer);

        puts(buffer);
        puts("\n");
    } else {
        int64 tid;
        if(tid = fork()) {
            join(tid);
        } else {
            InvokeCommand();
            exit(0);
        }
    }
    
    for(int i = 0; i < 128; i++)
        g_CmdBuffer[i] = 0;
    g_CmdBufferIndex = 0;
}

int main(int argc, char** argv) {

    if(argc >= 3 && strcmp(argv[1], "-c") == 0) {
        exec(argv[2], argc - 2, &argv[2]);

        puts("Command not found: ");
        puts(argv[2]);
        exit(1);
    }

    while(true) {
        puts("Test Shell > ");

        while(true) {
            char c;
            int64 count = read(stdinfd, &c, 1);
            if(count == 0)
                continue;

            if(c == '\b')
                HandleBackspace();
            else if(c == '\n') {
                HandleCommand();
                break;
            }
            else {
                g_CmdBuffer[g_CmdBufferIndex++] = c;
                write(stdoutfd, &c, 1);
            }
        }
    }

}