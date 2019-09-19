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

static void BuiltinEcho(int argc, char** argv) {
    for(int i = 1; i < argc; i++) {
        puts(argv[i]);
        puts(" ");
    }
    puts("\n");
}

static void BuiltinCat(int argc, char** argv) {
    if(argc != 2) {
        puts("Usage: cat <file>\n");
        return;
    }

    int64 fd = open(argv[1], open_mode_read);
    if(fd < 0) {
        puts("File not found\n");
        return;
    }

    char buffer[128];
    while(true) {
        int64 count = read(fd, buffer, sizeof(buffer));
        if(count <= 0)
            break;

        write(stdoutfd, buffer, count);
    }

    puts("\n");

    close(fd);
    exit(0);
}

static void BuiltinPut(int argc, char** argv) {
    if(argc < 2) {
        puts("Usage: put <file> ...\n");
        return;
    }

    int64 fd = open(argv[1], open_mode_write | open_mode_create | open_mode_clear);
    if(fd < 0) {
        puts("Failed not found\n");
        return;
    }

    for(int i = 2; i < argc; i++) {
        int l = strlen(argv[i]);

        write(fd, argv[i], l);
        write(fd, " ", 1);
    }

    close(fd);
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

    if(strcmp(argv[0], "echo") == 0) {
        BuiltinEcho(argc, argv);
    } else if(strcmp(argv[0], "cat") == 0) {
        BuiltinCat(argc, argv);
    } else if(strcmp(argv[0], "put") == 0) {
        BuiltinPut(argc, argv);
    } else {
        exec(argv[0], argc, argv);
        puts("Command not found\n");
    }

    exit(0);
}

static void HandleCommand() {
    puts("\n");

    if(g_CmdBufferIndex == 0)
        return;

    if(strcmp(g_CmdBuffer, "exit") == 0) {
        exit(0);
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