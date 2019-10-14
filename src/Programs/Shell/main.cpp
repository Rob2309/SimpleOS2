#include <simpleos_vfs.h>
#include <simpleos_process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Keys.h"

static char g_History[1024][128];
static int g_HistoryMinIndex = 1024;
static int g_HistoryIndex = 1024;

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

static void BuiltinTail(int argc, char** argv) {
    if(argc != 2) {
        puts("Usage: tail <file>\n");
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
        if(count < 0)
            break;

        write(stdoutfd, buffer, count);
    }
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
        
        statl(pathBuffer, &stats[i]);
    }

    for(int i = 0; i < numEntries; i++) {
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

        puts("    ");

        puts(entries[i].name);
        int p = strlen(entries[i].name);

        for(int i = p; i < 20; i++)
            puts(" ");

        switch(stats[i].type) {
        case NODE_FILE: puts("f "); break;
        case NODE_DIRECTORY: puts("d "); break;
        case NODE_DEVICE_CHAR: puts("c "); break;
        case NODE_DEVICE_BLOCK: puts("b "); break;
        case NODE_PIPE: puts("p "); break;
        case NODE_SYMLINK: puts("l "); break;
        }

        if(stats[i].type == NODE_SYMLINK) {
            puts("-> ");
            puts(stats[i].linkPath);
        }

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

static void BuiltinLN(int argc, char** argv) {
    if(argc < 3) {
        puts("Usage: ln [-s] <file> <link>\n");
        return;
    }

    bool symlink = false;

    int argi = 1;
    while(argi < argc) {
        if(strcmp(argv[argi], "-s") == 0) {
            symlink = true;
            argi++;
        } else {
            break;
        }
    }

    if(argi == argc) {
        puts("Usage: ln [-s] <file> <link>\n");
        return;
    }

    const char* path = argv[argi];
    argi++;

    if(argi == argc) {
        puts("Usage: ln [-s] <file> <link>\n");
        return;
    }

    const char* linkPath = argv[argi];
    argi++;

    if(symlink) {
        int64 error = create_symlink(path, linkPath);
        if(error != 0)
            puts("Failed to create symlink\n");
    } else {
        int64 error = create_hardlink(path, linkPath);
        if(error != 0)
            puts("Failed to create hardlink\n");
    }
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
    } else if(strcmp(argv[0], "ln") == 0) {
        BuiltinLN(argc, argv);
    } else if(strcmp(argv[0], "tail") == 0) {
        BuiltinTail(argc, argv);
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

    if(g_HistoryMinIndex == 1024 || strcmp(g_History[1023], g_CmdBuffer) != 0) {
        memcpy(&g_History[g_HistoryMinIndex - 1], &g_History[g_HistoryMinIndex], 128 * (1024 - g_HistoryMinIndex));
        strcpy(g_History[1023], g_CmdBuffer);
        g_HistoryMinIndex--;
    }

    if(strcmp(g_CmdBuffer, "exit") == 0) {
        exit(0);
    } else {
        int64 tid;
        if(tid = fork()) {
            while(true) {
                char c;
                int64 count = read(stdinfd, &c, 1);
                if(count != 0) {
                    if(c == '\3') {
                        kill(tid);
                        join(tid);
                        break;
                    }
                }

                if(try_join(tid) == 0)
                    break;
            }
        } else {
            InvokeCommand();
            exit(0);
        }
    }
    
    for(int i = 0; i < 128; i++)
        g_CmdBuffer[i] = 0;
    g_CmdBufferIndex = 0;
}

static bool g_ShiftLeft = false;
static bool g_ShiftRight = false;
static bool g_CtrlLeft = false;
static bool g_CtrlRight = false;

static char ConvertKey(uint16 key) {
    if((g_ShiftLeft || g_ShiftRight) && (g_CtrlLeft || g_CtrlRight))
        return '\0';

    if(g_ShiftLeft || g_ShiftRight) {
        if(key >= KEY_A && key <= KEY_Z)
            return 'A' + (key - KEY_A);

        switch(key) {
        case KEY_0: return '=';
        case KEY_1: return '!';
        case KEY_2: return '"';
        case KEY_3: return 0;
        case KEY_4: return '$';
        case KEY_5: return '%';
        case KEY_6: return '&';
        case KEY_7: return '/';
        case KEY_8: return '(';
        case KEY_9: return ')';

        case KEY_TAB: return ' ';
        case KEY_PLUS: return '*';
        case KEY_MINUS: return '_';
        case KEY_COMMA: return ';';
        case KEY_PERIOD: return ':';
        case KEY_SPACE: return ' ';
        }

        return '\0';
    } else if(g_CtrlLeft || g_CtrlRight) {
        return '\0';
    } else {
        if(key >= KEY_0 && key <= KEY_9)
            return '0' + (key - KEY_0);
        if(key >= KEY_A && key <= KEY_Z)
            return 'a' + (key - KEY_A);

        switch(key) {
        case KEY_TAB: return ' ';
        case KEY_PLUS: return '+';
        case KEY_MINUS: return '-';
        case KEY_COMMA: return ',';
        case KEY_PERIOD: return '.';
        case KEY_SPACE: return ' ';
        }

        return '\0';
    }
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
            uint16 k;
            int64 count = read(stdinfd, &k, sizeof(uint16));
            if(count == 0)
                continue;

            if(k & KEY_RELEASED) {
                k &= 0x7FFF;
                
                if(k == KEY_SHIFT_LEFT)
                    g_ShiftLeft = false;
                else if(k == KEY_SHIFT_RIGHT)
                    g_ShiftRight = false;
                else if(k == KEY_CTRL_LEFT)
                    g_CtrlLeft = false;
                else if(k == KEY_CTRL_RIGHT)
                    g_CtrlRight = false;
            } else if(k == KEY_BACKSPACE) {
                g_HistoryIndex = 1024;
                HandleBackspace();
            } else if(k == KEY_ENTER) {
                g_HistoryIndex = 1024;
                HandleCommand();
                break;
            } else if(k == KEY_UP) {
                if(g_HistoryIndex > g_HistoryMinIndex) {
                    for(int i = 0; i < strlen(g_CmdBuffer); i++)
                        puts("\b");

                    g_HistoryIndex--;
                    strcpy(g_CmdBuffer, g_History[g_HistoryIndex]);
                    g_CmdBufferIndex = strlen(g_CmdBuffer);

                    puts(g_CmdBuffer);
                }
            } else if(k == KEY_DOWN) {
                if(g_HistoryIndex < 1023) {
                    for(int i = 0; i < strlen(g_CmdBuffer); i++)
                        puts("\b");

                    g_HistoryIndex++;
                    strcpy(g_CmdBuffer, g_History[g_HistoryIndex]);
                    g_CmdBufferIndex = strlen(g_CmdBuffer);

                    puts(g_CmdBuffer);
                } else if(g_HistoryIndex == 1023) {
                    for(int i = 0; i < strlen(g_CmdBuffer); i++)
                        puts("\b");

                    g_HistoryIndex = 1024;
                    for(int i = 0; i < 128; i++)
                        g_CmdBuffer[i] = 0;
                    g_CmdBufferIndex = 0;
                }
            } else if(k == KEY_SHIFT_LEFT)
                g_ShiftLeft = true;
            else if(k == KEY_SHIFT_RIGHT)
                g_ShiftRight = true;
            else if(k == KEY_CTRL_LEFT)
                g_CtrlLeft = true;
            else if(k == KEY_CTRL_RIGHT)
                g_CtrlRight = true; 
            else {
                g_HistoryIndex = 1024;
                char c = ConvertKey(k);
                
                if(c != '\0') {
                    g_CmdBuffer[g_CmdBufferIndex++] = c;
                    write(stdoutfd, &c, 1);
                }
            }
        }
    }

}