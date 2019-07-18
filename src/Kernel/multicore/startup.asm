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
        mov edi, [ds: smp_TrampolineBaseAddress - smp_Start]

        mov ecx, edi
        add ecx, GDT - smp_Start                        ; Calculate absolute address of temp GDT
        mov DWORD [ds: GDT_Desc.base - smp_Start], ecx  ; Populate GDT_Desc with correct GDT address
        lgdt [ds: GDT_Desc - smp_Start]
        
        mov sp, stack_top - smp_Start                   ; Set up temp Stack

        mov ecx, edi
        add ecx, pm_Entry - smp_Start                   ; Calculate absolute address of pm_Entry
        push DWORD 0x08
        push DWORD ecx

        mov ecx, cr0
        or ecx, 1
        mov cr0, ecx

        o32 a32 retf

; Temporary GDT before the real one is available
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
    .code64:
        DW 0xFFFF
        DW 0x0000
        DB 0x00
        DB 0b10011010
        DB 0b10101111
        DB 0x00
    .data64:
        DW 0xFFFF
        DW 0x0000
        DB 0x00
        DB 0b10010010
        DB 0b10001111
        DB 0x00

GDT_Desc:
    .limit:
		DW 5 * 8 - 1
    .base:
		DD 0

%macro Variable 1
    GLOBAL %1
    %1: DQ 0
%endmacro

Variable smp_TrampolineBaseAddress
Variable smp_PML4Address
Variable smp_StackAddress
Variable smp_DestinationAddress

; a very very small temporary stack
TIMES 20 DB 0
stack_top:

[BITS 32]
pm_Entry:
        mov eax, 16
        mov ds, eax
        mov es, eax
        mov ss, eax

        ; Setup real stack
        mov eax, edi
        add eax, stack_top - smp_Start
        mov esp, eax

        ; Set PML4
        mov eax, DWORD [edi + smp_PML4Address - smp_Start]
        mov cr3, eax

        ; Enable PAE paging
        mov eax, cr4
        or eax, 1 << 5
        mov cr4, eax

        ; Enable long mode
        mov ecx, 0xC0000080
        rdmsr
        or eax, 1 << 8
        wrmsr

        ; Enable paging
        mov eax, cr0
        or eax, 1 << 31
        mov cr0, eax

        ; Jump from compatibility mode to actual long mode
        mov eax, edi
        add eax, lm_Entry - smp_Start
        push DWORD 24
        push DWORD eax
        retf

    .loop:
        hlt
        jmp .loop

[BITS 64]
lm_Entry:
        ; Load 64-Bit data segments
        mov rax, 32
        mov ds, rax
        mov es, rax
        mov ss, rax

        ; Setup stack
        mov rax, rdi
        add rax, smp_StackAddress - smp_Start
        mov rsp, [rax]

        ; Jump to CoreEntry function
        mov rax, rdi
        add rax, smp_DestinationAddress - smp_Start
        jmp [rax]

    .loop:
        hlt
        jmp .loop

GLOBAL smp_End
smp_End: