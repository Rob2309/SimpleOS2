cpp_compiler := $(ELF_GCC)
asm_compiler := 

libs := $(bin_dir)/libc/libc.a

int_dir := $(int_dir)/Programs/Shell
bin_dir := $(bin_dir)/Programs/Shell

includes := ../../libc

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti
link_flags := -g -nostdlib -e __start -static -lgcc

out_file := shell.elf

include ../../makeinclude/cpp.mk