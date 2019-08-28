

SECTIONS
{

    .text :
    {
        *(.text)
        *(.text.*)
    }

    .rodata :
    {
        SYSCALL_ARRAY_START = . ;
        *(.syscall_array) 
        SYSCALL_ARRAY_END = . ;

        KINIT_ARRAY_START = . ;
        *(.kinit_array)
        KINIT_ARRAY_END = . ;
    }

    .data.rel.ro :
    {
        *(.data.rel.ro)
        *(.data.rel.ro.*)
    }

}