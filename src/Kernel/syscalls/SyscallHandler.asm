[BITS 64]

EXTERN SyscallDispatcher

GLOBAL SyscallEntry
SyscallEntry:
    ; rdi = function
    ; rsi = arg1
    ; rdx = arg2
    ; RCX = userrip
    ; r8 = arg3
    ; r9 = arg4
    ; R11 = userflags
    
    swapgs
    sti

    mov r10, rsp            ; Save user stack
    mov rsp, [gs:0]         ; Load Kernel rsp

    push r15
    push r14
    push r13
    push r12
    push rbx
    push r11
    push rbp
    push r10
    push rcx

    mov rcx, r8 ; shift arguments
    mov r8, r9
    mov r9, rsp

    call SyscallDispatcher wrt ..plt

    pop rcx
    pop r10
    pop rbp
    pop r11
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15

    mov rsp, r10

    cli
    swapgs

    o64 sysret