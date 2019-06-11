int_dir := $(int_dir)/Kernel
bin_dir := $(bin_dir)/Kernel

include_flags := -I. -I../common

c_headers := $(shell find . -type f -name "*.h" -printf "%p ") $(shell find ../common -type f -name "*.h" -printf "%p ")
cross_headers := $(shell find . -type f -name "*.inc" -printf "%p ")

c_sources := $(shell find . -type f -name "*.c" -printf "%p ")
cpp_sources := $(shell find . -type f -name "*.cpp" -printf "%p ")
asm_sources := $(shell find . -type f -name "*.asm" -printf "%p ")

c_objects := $(addprefix $(int_dir)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(addprefix $(int_dir)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(addprefix $(int_dir)/, $(subst .asm,.asm.o, $(asm_sources)))

compile_flags := -g -fPIC -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -fPIC -nostdlib -shared

ifeq ($(config),Debug)
	compile_flags += -DDEBUG -O0
	link_flags += -O0
endif
ifeq ($(config),Release)
	compile_flags += -DRELEASE -O2
	link_flags += -O2
endif

$(bin_dir)/Kernel.sys: $(c_objects) $(cpp_objects) $(asm_objects)
	@ echo "\e[33mLinking Kernel executable\e[0m"
	@ mkdir -p $(dir $@)
	@ $(ELF_GCC) $(link_flags) -o $@ $^ -e main -static -lgcc

$(int_dir)/%.c.o: ./%.c $(c_headers) $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(ELF_GCC) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.cpp.o: ./%.cpp $(c_headers) $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(ELF_GCC) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.asm.o: ./%.asm $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(NASM) -g -f elf64 $< -o $@