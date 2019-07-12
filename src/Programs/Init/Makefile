cpp_compiler := $(ELF_GCC)
asm_compiler := 

int_dir := $(int_dir)/Programs/Init
bin_dir := $(bin_dir)/Programs/Init

includes := ../../common

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib -e main -static -lgcc

out_file := Init.elf

include ../../makeinclude/cpp.mk