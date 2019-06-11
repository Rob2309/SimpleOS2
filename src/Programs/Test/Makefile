cpp_compiler := $(ELF_GCC)
asm_compiler := 

int_dir := $(int_dir)/Programs/Test
bin_dir := $(bin_dir)/Programs/Test

includes := ../../common

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib -e main -static -lgcc

out_file := Test.elf

include ../../makeinclude/cpp.mk