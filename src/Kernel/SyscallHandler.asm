[BITS 64]

EXTERN SyscallDispatcher

GLOBAL SyscallEntry
SyscallEntry:
    ; RCX = userrip
    ; R11 = userflags

    ; rdi = function
    ; rsi = arg1
    ; rdx = arg2
    ; r8 = arg3
    ; r9 = arg4
    
    push rcx
    push r11
    push r15
    mov r15, rsp            ; Save user stack
    mov rsp, QWORD [gs:0]   ; Load Kernel rsp

    ; shift arguments to appropriate registers
    mov rcx, r8
    mov r8, r9

    call SyscallDispatcher wrt ..plt

    mov rsp, r15
    pop r15
    pop r11
    pop rcx

    o64 sysret