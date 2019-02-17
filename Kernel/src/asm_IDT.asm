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

    mov ax, 0x10
    mov ss, ax

    mov ax, ds
    push rax

    mov rax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, rsp
    call ISRCommonHandler wrt ..plt

    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popAll

    add rsp, 16
    sti
    o64 a64 iret

%macro ISR_NOERROR 1
    GLOBAL ISRSTUB_%1
    ISRSTUB_%1:
        cli
        push QWORD 0 
        push QWORD %1
        jmp ISRCommon
%endmacro

%macro ISR_ERROR 1
    GLOBAL ISRSTUB_%1
    ISRSTUB_%1:
        cli
        push QWORD %1
        jmp ISRCommon
%endmacro

%define ISRSTUB(vectno) ISR_NOERROR vectno
%define ISRSTUBE(vectno) ISR_ERROR vectno
%include "Kernel/src/ISR.inc"