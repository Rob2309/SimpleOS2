cpp_compiler := $(ELF_GCC)
asm_compiler := $(NASM) -g -f elf64

libs := $(bin_dir)/acpica/acpica.a

int_dir := $(int_dir)/Kernel
bin_dir := $(bin_dir)/Kernel

includes := ../acpica/include

compile_flags := -g -fPIC -ffreestanding -fno-stack-protector -fno-exceptions -mno-sse -fno-rtti -DACPI_LIBRARY
link_flags := -g -fPIC -nostdlib -shared -e main -static -lgcc -T linker.ld -Wl,--no-undefined

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