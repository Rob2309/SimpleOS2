#include <simpleos_process.h>
#include <simpleos_string.h>
#include <simpleos_inout.h>
#include <simpleos_vfs.h>
#include <simpleos_kill.h>

struct UserInfo {
    uint64 uid;
    uint64 gid;
    const char* username;
    const char* pw;
};

static UserInfo g_Users[] = {
    { 0, 0, "root", "123456" },
    { 100, 100, "testuser", "pw1234" },
    { 101, 101, "alice", "bad4321" }
};

static char g_UserNameBuffer[256] = { 0 };
static char g_PasswordBuffer[256] = { 0 };

static void GetInputLine(char* buffer, bool show) {
    int64 length = 0;

    while(true) {
        char c;
        int64 count = read(stdinfd, &c, sizeof(c));
        if(count == 0)
            continue;

        if(c == '\n') {
            buffer[length] = '\0';
            puts("\n");
            break;
        } else if(c == '\b') {
            if(length != 0) {
                buffer[length - 1] = '\0';
                length--;
                if(show)
                    puts("\b");
            }
        } else {
            buffer[length] = c;
            length++;

            if(show)
                write(stdoutfd, &c, sizeof(c));
        }
    }
}

static void RunUser(uint64 uid, uint64 gid) {
    int64 tid;
    if((tid = fork()) != 0) {
        join(tid);
        devcmd(stdoutfd, 1, (void*)gettid());
    } else {
        devcmd(stdoutfd, 1, (void*)gettid());
        changeuser(uid, gid);
        exec("/boot/Shell.elf", 0, nullptr);
    }
}

static void HandleKill() {
    puts("\n");
    exec("/boot/Login.elf", 0, nullptr);
}

int main(int argc, char** argv)
{
    setkillhandler(HandleKill);
    devcmd(stdoutfd, 1, (void*)gettid());

    while(true) {
        puts("Username: ");

        GetInputLine(g_UserNameBuffer, true);

        const UserInfo* user = nullptr;
        for(const auto& u : g_Users) {
            if(strcmp(u.username, g_UserNameBuffer) == 0) {
                user = &u;
                break;
            }
        }
        if(user == nullptr) {
            puts("Unknown user\n");
            continue;
        }

        puts("Password: ");
        GetInputLine(g_PasswordBuffer, false);

        if(strcmp(g_PasswordBuffer, user->pw) == 0) {
            RunUser(user->uid, user->gid);
        } else {
            puts("Login failed\n");
        }
    }
}