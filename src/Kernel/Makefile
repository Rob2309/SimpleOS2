cpp_compiler := $(ELF_GCC)
asm_compiler := $(NASM) -g -f elf64

int_dir := $(int_dir)/Kernel
bin_dir := $(bin_dir)/Kernel

includes := ../common

compile_flags := -g -fPIC -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -fPIC -nostdlib -shared -e main -static -lgcc

ifeq ($(config),Debug)
	compile_flags += -DDEBUG -O0
	link_flags += -O0
endif
ifeq ($(config),Release)
	compile_flags += -DRELEASE -O2
	link_flags += -O2
endif

out_file := Kernel.sys

include ../makeinclude/cpp.mk