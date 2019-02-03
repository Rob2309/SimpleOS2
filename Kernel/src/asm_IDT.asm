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

[GLOBAL LIDT]
[EXTERN g_IDTDesc]
LIDT:
    lea rax, [rel g_IDTDesc wrt ..got]
    ;lidt [rax]
    mov bx, [rax]
    xor rax, rax
    mov ax, bx
    ret

;[EXTERN ISRCommonHandler]
[GLOBAL ISRCommonHandler]
[EXTERN printf]
ISRCommonHandler:
    ;lea rdi, [rel .msg]
    ;mov rsi, 42
    ;call printf wrt ..plt
    ret

    .msg:
        DB "Interrupt %i", 0


ISRCommon:
    pushAll

    mov ax, ds
    push rax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ;call ISRCommonHandler

    pop rbx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    popAll

    add rsp, 16
    sti
    iret

%macro ISR_NOERRCODE 1
  global ISR%1
  ISR%1:
    cli
    push byte 0 
    push byte %1
    add rsp, 16
    a64 o64 iret
    jmp ISRCommon
%endmacro

%macro ISR_ERRCODE 1
  global ISR%1
  ISR%1:
    cli
    push byte %1
    add rsp, 16
    a64 o64 iret
    jmp ISRCommon
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31