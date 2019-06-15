[BITS 16]

GLOBAL smp_Start
smp_Start:
GLOBAL smp_Trampoline
smp_Trampoline:
        cli
        
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax

        ; Calculate absolute address of smp_Start
        mov eax, [ds: smp_Address - smp_Start]

        mov ecx, eax
        add ecx, GDT - smp_Start                        ; Calculate absolute address of temp GDT
        mov DWORD [ds: GDT_Desc.base - smp_Start], ecx  ; Populate GDT_Desc with correct GDT address
        lgdt [ds: GDT_Desc - smp_Start]
        
        mov sp, stack_top - smp_Start                   ; Set up temp Stack

        mov ecx, eax
        add ecx, pm_Entry - smp_Start                   ; Calculate absolute address of pm_Entry
        push DWORD 0x08
        push DWORD ecx

        mov ecx, cr0
        or ecx, 1
        mov cr0, ecx

        o32 a32 retf

GDT:
    .null:
        DD 0
        DD 0
    .code:
        DW 0xFFFF
        DW 0x0000
        DB 0x00
        DB 0b10011010
        DB 0b11001111
        DB 0x00
    .data:
        DW 0xFFFF
        DW 0x0000
        DB 0x00
        DB 0b10010010
        DB 0b11001111
        DB 0x00

GDT_Desc:
    .limit:
		DW 3 * 8 - 1
    .base:
		DD 0

GLOBAL smp_StartupFlag
smp_StartupFlag:
    DD 0

GLOBAL smp_Address
smp_Address:
    DQ 0

TIMES 512 DB 0
stack_top:

[BITS 32]
pm_Entry:
        mov edx, 16
        mov ds, edx
        mov es, edx
        mov ss, edx

        mov ecx, eax
        add ecx, smp_StartupFlag - smp_Start
        mov WORD [ecx], 0xDEAD

    .loop:
        hlt
        jmp .loop

GLOBAL smp_End
smp_End: