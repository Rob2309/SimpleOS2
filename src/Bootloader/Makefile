int_dir := $(int_dir)/Bootloader
bin_dir := $(bin_dir)/Bootloader

include_flags := -I. -I../common -I../../dep/gnu-efi/inc -I../../dep/gnu-efi/inc/x86_64

c_headers := $(shell find . -type f -name "*.h" -printf "%p ") $(shell find ../common -type f -name "*.h" -printf "%p ")
cross_headers := $(shell find . -type f -name "*.inc" -printf "%p ")

c_sources := $(shell find . -type f -name "*.c" -printf "%p ")
cpp_sources := $(shell find . -type f -name "*.cpp" -printf "%p ")
asm_sources := $(shell find . -type f -name "*.asm" -printf "%p ")

c_objects := $(addprefix $(int_dir)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(addprefix $(int_dir)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(addprefix $(int_dir)/, $(subst .asm,.asm.o, $(asm_sources)))

compile_flags := -ffreestanding
link_flags := -nostdlib -Wl,-dll -shared -Wl,--subsystem,10

$(bin_dir)/BOOTX64.EFI: $(c_objects) $(cpp_objects) $(asm_objects)
	@mkdir -p $(dir $@)
	$(PE_GCC) $(link_flags) -o $@ $^ -e efi_main

$(int_dir)/%.c.o: ./%.c $(c_headers) $(cross_headers)
	@mkdir -p $(dir $@)
	$(PE_GCC) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.cpp.o: ./%.cpp $(c_headers) $(cross_headers)
	@mkdir -p $(dir $@)
	$(PE_GCC) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.asm.o: ./%.asm $(cross_headers)
	@mkdir -p $(dir $@)
	$(NASM) -g -f elf64 $< -o $@