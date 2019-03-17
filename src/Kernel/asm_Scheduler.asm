[BITS 64]

GLOBAL IdleProcess
IdleProcess:
    .loop:
        hlt
        jmp .loop