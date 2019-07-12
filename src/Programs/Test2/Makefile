cpp_compiler := $(ELF_GCC)
asm_compiler := 

int_dir := $(int_dir)/Programs/Test2
bin_dir := $(bin_dir)/Programs/Test2

includes := ../../common ../../libc

libs := $(bin_dir)/../../libc/libc.a

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib -e main -static -lgcc

out_file := Test2.elf

include ../../makeinclude/cpp.mk