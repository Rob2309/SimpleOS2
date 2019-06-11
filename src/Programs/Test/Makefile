int_dir := $(int_dir)/Programs/Test
bin_dir := $(bin_dir)/Programs/Test

include_flags := -I. -I../../common

c_headers := $(shell find . -type f -name "*.h" -printf "%p ") $(shell find ../../common -type f -name "*.h" -printf "%p ")
cross_headers := $(shell find . -type f -name "*.inc" -printf "%p ")

c_sources := $(shell find . -type f -name "*.c" -printf "%p ")
cpp_sources := $(shell find . -type f -name "*.cpp" -printf "%p ")
asm_sources := $(shell find . -type f -name "*.asm" -printf "%p ")

c_objects := $(addprefix $(int_dir)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(addprefix $(int_dir)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(addprefix $(int_dir)/, $(subst .asm,.asm.o, $(asm_sources)))

compile_flags := -g -ffreestanding -fno-stack-protector -fno-exceptions
link_flags := -g -nostdlib

$(bin_dir)/test.elf: $(c_objects) $(cpp_objects) $(asm_objects)
	@ echo "\e[33mLinking Test executable\e[0m"
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