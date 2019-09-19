cpp_compiler := $(ELF_GCC)
asm_compiler := 

libs := $(bin_dir)/libc/libc.a

int_dir := $(int_dir)/Programs/Cat
bin_dir := $(bin_dir)/Programs/Cat

includes := ../../libc

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib -e __start -static -lgcc

ifeq ($(config),Release)
	compile_flags += -DRELEASE -O2
	link_flags += -O2
endif

out_file := cat

include ../../makeinclude/cpp.mk