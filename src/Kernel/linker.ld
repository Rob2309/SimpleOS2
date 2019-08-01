
SECTIONS
{
    .rodata.syscall_array :
    { 
        SYSCALL_ARRAY_START = . ;
        *(.syscall_array) 
        SYSCALL_ARRAY_END = . ;
    }

    .rodata.kinit_array :
    {
        KINIT_ARRAY_START = . ;
        *(.kinit_array)
        KINIT_ARRAY_END = . ;
    }
}
INSERT AFTER .rodata;