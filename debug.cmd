set architecture i386:x86-64
target remote tcp::26000

watch {unsigned long long}0x1000
c

set $ktext = {unsigned long long}0x1000
add-symbol-file build/dbgSession/Kernel.sys.dbg $ktext

layout split

b main
c