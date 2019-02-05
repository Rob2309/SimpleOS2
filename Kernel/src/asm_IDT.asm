[BITS 64]

%macro pushAll 0
    push rax
    push rcx
    push rdx
    push rbx
    push rsp
    push rbp
    push rsi
    push rdi
%endmacro

%macro popAll 0
    pop rdi
    pop rsi
    pop rbp
    pop rsp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

[EXTERN ISRCommonHandler]

ISRCommon:
    pushAll

    mov ax, ds
    push rax

    mov rax, 16
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

%macro ISR_NOERROR 2
    GLOBAL %1
    %1:
        cli
        push QWORD 0 
        push QWORD %2
        jmp ISRCommon
%endmacro

%macro ISR_ERROR 2
    GLOBAL %1
    %1:
        cli
        push QWORD %2
        jmp ISRCommon
%endmacro

%define ISR(name, vectno) ISR_NOERROR name, vectno
%define ISRE(name, vectno) ISR_ERROR name, vectno
%include "ISR.inc"