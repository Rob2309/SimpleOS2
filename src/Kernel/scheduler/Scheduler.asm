[BITS 64]

GLOBAL IdleThread
IdleThread:
    .loop:
        hlt
        jmp .loop

GLOBAL GoToThread
GoToThread:
        mov rsp, rdi

        pop rax
        mov ds, ax
        mov es, ax

        pop r15
        pop r14
        pop r13
        pop r12
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        pop rdx
        pop rcx
        pop rax

        add rsp, 16
        iretq

GLOBAL SwitchThread
SwitchThread:
        mov rax, [rsp]          ; save return address
        mov [rdi + 18 * 8], rax

        mov [rdi + 0 * 8], ds
        mov [rdi + 19 * 8], cs
        mov [rdi + 22 * 8], ss

        mov r10, rsp            ; save return rsp
        add r10, 8
        mov [rdi + 21 * 8], r10

        pushfq                  ; save flags
        pop rax
        mov [rdi + 20 * 8], rax

        mov [rdi + 1 * 8], r15
        mov [rdi + 2 * 8], r14
        mov [rdi + 3 * 8], r13
        mov [rdi + 4 * 8], r12
        mov [rdi + 11 * 8], rbp
        mov [rdi + 12 * 8], rbx


        mov rsp, rsi

        pop rax
        mov ds, rax
        mov es, rax

        pop r15
        pop r14
        pop r13
        pop r12
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        pop rdx
        pop rcx
        pop rax

        add rsp, 16
        iretq