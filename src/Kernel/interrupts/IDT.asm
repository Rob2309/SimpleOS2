[BITS 64]

%macro pushAll 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popAll 0
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
%endmacro

[EXTERN ISRCommonHandler]

ISRCommon:
    pushAll

    mov rax, 0x10    ; kernel data selector
    mov ss, ax

    mov ax, ds      ; save old ds
    push rax

    mov rax, 0x10   ; load kernel data selectors
    mov ds, ax
    mov es, ax

    ; The stack will be 16-Byte aligned at this point (important for gcc x64 ABI)

    mov rdi, rsp                    ; rdi is the first function argument, thus has to hold the address of the IDT::Registers struct (see IDT.h)

    sub rsp, 8
    inc QWORD [rsp]

    mov rsi, [rsp]

    call ISRCommonHandler wrt ..plt ; call the C interrupt handler

    dec QWORD [rsp]
    add rsp, 8

    pop rax         ; restore old data selectors
    mov ds, ax
    mov es, ax

    popAll

    add rsp, 16     ; pop error code and interrupt number from stack
    o64 a64 iret

%macro ISR_NOERROR 1
    GLOBAL ISRSTUB_%1
    ISRSTUB_%1:
        cli
        push QWORD 0    ; push fake error code
        push QWORD %1   ; push interrupt number
        jmp ISRCommon
%endmacro

%macro ISR_ERROR 1
    GLOBAL ISRSTUB_%1
    ISRSTUB_%1:
        cli
        push QWORD %1   ; push interrupt number
        jmp ISRCommon
%endmacro

%define ISRSTUB(vectno) ISR_NOERROR vectno
%define ISRSTUBE(vectno) ISR_ERROR vectno
%include "interrupts/ISR.inc"