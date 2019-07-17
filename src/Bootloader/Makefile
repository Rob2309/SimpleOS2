cpp_compiler := $(PE_GCC)
asm_compiler := 

int_dir := $(int_dir)/Bootloader
bin_dir := $(bin_dir)/Bootloader

includes := ../../dep/gnu-efi/inc ../../dep/gnu-efi/inc/x86_64

compile_flags := -ffreestanding
link_flags := -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

out_file := BOOTX64.EFI

include ../makeinclude/cpp.mk