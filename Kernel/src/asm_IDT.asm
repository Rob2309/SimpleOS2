[BITS 64]

%macro pushAll 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
%endmacro

%macro popAll 0
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
%include "ISR.inc"