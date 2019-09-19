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

    bool quotedMode = false;
    if(*buf == '"') {
        buf++;
        quotedMode = true;
    }

    char* res = buf;

    while(true) {
        if(*buf == ' ' && !quotedMode) {
            *buf = '\0';
            buf++;
            return res;
        }
        if(*buf == '"' && quotedMode) {
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
        puts("Usage: put [-a] <file> ...\n");
        return;
    }

    bool append = false;

    int argi = 1;
    while(argi < argc) {
        if(strcmp(argv[argi], "-a") == 0) {
            append = true;
            argi++;
        } else {
            break;
        }
    }

    if(argi == argc) {
        puts("Usage: put [-a] <file> ...\n");
        return;
    }

    const char* path = argv[argi];
    argi++;

    uint64 mode = open_mode_write | open_mode_create;
    if(!append)
        mode |= open_mode_clear;
    int64 fd = open(path, mode);
    if(fd < 0) {
        puts("File not found\n");
        return;
    }

    if(append)
        seekfd(fd, seek_mode_end, 0);

    for(int i = argi; i < argc; i++) {
        int l = strlen(argv[i]);

        write(fd, argv[i], l);
        if(i < argc - 1)
            write(fd, " ", 1);
    }

    close(fd);
}

static void BuiltinLS(int argc, char** argv) {
    if(argc < 2) {
        puts("Usage: ls <folder>\n");
        return;
    }

    int numEntries = 10;
    ListEntry* entries = (ListEntry*)malloc(numEntries * sizeof(ListEntry));
    Stats* stats = (Stats*)malloc(numEntries * sizeof(Stats));

    int64 error = list(argv[1], &numEntries, entries);
    if(error != 0) {
        puts("File not found\n");
        return;
    }

    if(numEntries > 10) {
        free(entries);
        free(stats);
        entries = (ListEntry*)malloc(numEntries * sizeof(ListEntry));
        stats = (Stats*)malloc(numEntries * sizeof(Stats));

        error = list(argv[1], &numEntries, entries);
        if(error != 0) {
            puts("File not found\n");
            return;
        }
    }

    char pathBuffer[256];
    strcpy(pathBuffer, argv[1]);
    int l = strlen(pathBuffer);
    pathBuffer[l] = '/';
    l++;

    for(int i = 0; i < numEntries; i++) {
        strcpy(&pathBuffer[l], entries[i].name);
        
        stat(pathBuffer, &stats[i]);
    }

    for(int i = 0; i < numEntries; i++) {
        puts(entries[i].name);
        puts("  ");

        switch(stats[i].type) {
        case NODE_FILE: puts("f "); break;
        case NODE_DIRECTORY: puts("d "); break;
        case NODE_DEVICE_CHAR: puts("c "); break;
        case NODE_DEVICE_BLOCK: puts("b "); break;
        case NODE_PIPE: puts("p "); break;
        }

        if(stats[i].perms.owner & PermRead)
            puts("r");
        else
            puts("-");
        if(stats[i].perms.owner & PermWrite)
            puts("w");
        else
            puts("-");

        if(stats[i].perms.group & PermRead)
            puts("r");
        else
            puts("-");
        if(stats[i].perms.group & PermWrite)
            puts("w");
        else
            puts("-");

        if(stats[i].perms.other & PermRead)
            puts("r");
        else
            puts("-");
        if(stats[i].perms.other & PermWrite)
            puts("w");
        else
            puts("-");

        puts("\n");
    }

    free(entries);
}

static void BuiltinMkdir(int argc, char** argv) {
    if(argc < 2) {
        puts("Usage: mkdir <folder>\n");
        return;
    }

    int64 error = create_folder(argv[1]);
    if(error != 0)
        puts("Failed to create folder\n");
}

static void BuiltinRM(int argc, char** argv) {
    if(argc < 2) {
        puts("Usage: rm <path>\n");
        return;
    }

    int64 error = delete_file(argv[1]);
    if(error != 0)
        puts("Failed to delete path\n");
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
    } else if(strcmp(argv[0], "ls") == 0) {
        BuiltinLS(argc, argv);
    } else if(strcmp(argv[0], "mkdir") == 0) {
        BuiltinMkdir(argc, argv);
    } else if(strcmp(argv[0], "rm") == 0) {
        BuiltinRM(argc, argv);
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