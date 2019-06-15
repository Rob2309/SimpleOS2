[BITS 16]

ALIGN 4096
GLOBAL func_CoreStartup
func_CoreStartup:
        cli
        mov WORD [0x1234], 0xDEAD
    .loop:
        hlt
        jmp .loop

GLOBAL func_CoreStartup_End
func_CoreStartup_End: