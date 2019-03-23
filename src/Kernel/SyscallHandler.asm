[BITS 64]

EXTERN SyscallDispatcher

GLOBAL SyscallEntry
SyscallEntry:
    ; RCX = userrip
    ; R11 = userflags
    push rcx
    push r11
    push r15
    push r14
    push r13
    push r12
    push r10
    push r9
    push r8
    push rbp
    push rsi
    push rdi
    push rdx
    push rbx
    push rax

    