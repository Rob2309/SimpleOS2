cpp_compiler := $(ELF_GCC)
asm_compiler := 

libs := $(bin_dir)/libc/libc.a

int_dir := $(int_dir)/Programs/Init
bin_dir := $(bin_dir)/Programs/Init

includes := ../../libc

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib -e __start -static -lgcc

out_file := init.elf

include ../../makeinclude/cpp.mk