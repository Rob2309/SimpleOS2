#include <simpleos_vfs.h>
#include <simpleos_process.h>
#include <stdio.h>

static char g_CmdBuffer[128] = { 0 };
static int g_CmdBufferIndex = 0;

static void HandleBackspace() {
    if(g_CmdBufferIndex > 0) {
        g_CmdBufferIndex--;
        g_CmdBuffer[g_CmdBufferIndex] = '\0';
        puts("\b");
    }
}

static void HandleCommand() {
    puts("\n");

    if(g_CmdBufferIndex == 0)
        return;

    int64 stdoutRead, stdoutWrite;
    pipe(&stdoutRead, &stdoutWrite);

    int64 tid;
    if(tid = fork()) {
        while(true);
        close(stdoutWrite);

        while(true) {
            char buffer[128];
            int64 count = read(stdoutRead, buffer, sizeof(buffer)-1);
            buffer[count] = '\0';
            puts(buffer);
        }
    } else {
        while(true);

        close(stdoutRead);
        copyfd(stdoutfd, stdoutWrite);

        exec(g_CmdBuffer);

        puts("Command not found\n");
        thread_exit(1);
    }
    
    for(int i = 0; i < 128; i++)
        g_CmdBuffer[i] = 0;
    g_CmdBufferIndex = 0;
}

int main() {

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