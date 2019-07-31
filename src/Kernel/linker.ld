
SECTIONS
{
    .rodata.syscall_array :
    { 
        SYSCALL_ARRAY_START = . ;
        *(.syscall_array) 
        SYSCALL_ARRAY_END = . ;
    }
}
INSERT AFTER .rodata;