[BITS 64]

GLOBAL IdleThread
IdleThread:
    .loop:
        hlt
        jmp .loop

GLOBAL ReturnToThread
ReturnToThread:
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
        o64 a64 iret