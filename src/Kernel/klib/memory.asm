[BITS 64]

EXTERN ThreadSetPageFaultRip

GLOBAL _kmemcpy_usersafe
_kmemcpy_usersafe:
        ; rdi = userDest
        ; rsi = src
        ; rdx = count
        push rdi
        push rsi
        push rdx
        ; stack 16 byte aligned

        mov rdi, .error
        call ThreadSetPageFaultRip wrt ..plt

        pop rdx
        pop rsi
        pop rdi

        mov rcx, rdx
        rep movsb

        mov rdi, 0
        call ThreadSetPageFaultRip wrt ..plt

        mov rax, 1
        ret

    .error:
        mov rdi, 0
        call ThreadSetPageFaultRip wrt ..plt

        mov rax, 0
        ret

GLOBAL _kmemset_usersafe
_kmemset_usersafe:
        ; rdi = dest
        ; rsi = val
        ; rdx = count
        push rdi
        push rsi
        push rdx
        ; stack 16 byte aligned

        mov rdi, .error
        call ThreadSetPageFaultRip wrt ..plt

        pop rdx
        pop rsi
        pop rdi

        mov rcx, rdx
        mov rax, rsi
        rep stosb

        mov rdi, 0
        call ThreadSetPageFaultRip wrt ..plt

        mov rax, 1
        ret

    .error:
        mov rdi, 0
        call ThreadSetPageFaultRip wrt ..plt

        mov rax, 0
        ret