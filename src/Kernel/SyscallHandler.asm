[BITS 64]

EXTERN SyscallDispatcher

GLOBAL SyscallEntry
SyscallEntry:
    ; RCX = userrip
    ; R11 = userflags
    
    mov r10, rsp
    mov rsp, QWORD [gs:0] ; Load Kernel rsp

    push rcx ; return rip
    push r11 ; return rflags
    push r10 ; return rsp

    push r15
    push r14
    push r13
    push r12
    push r9
    push r8
    push rbp
    push rsi
    push rdi
    push rdx
    push rbx
    push rax

    mov rdi, rsp

    sub rsp, 8  ; align stack on 16-Byte boundary (assuming Kernel stack is 16-Byte aligned)

    call SyscallDispatcher wrt ..plt

    add rsp, 8

    pop rax
    pop rbx
    pop rdx
    pop rdi
    pop rsi
    pop rbp
    pop r8
    pop r9
    pop r12
    pop r13
    pop r14
    pop r15

    pop r10
    pop r11
    pop rcx

    mov rsp, r10

    o64 sysret