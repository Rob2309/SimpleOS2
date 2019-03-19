int_dir := $(subst src/,int/$(config)/, $(src_dir))

elf_include_flags := $(addprefix -I, src/common)
elf_nasm_include_flags := $(addprefix -i, src/common)

c_headers := $(wildcard $(src_dir)/*.h) $(wildcard src/common/*.h)
cross_headers := $(wildcard $(src_dir)/*.inc) $(wildcard src/common/*.inc)

c_sources := $(wildcard $(src_dir)/*.c)
cpp_sources := $(wildcard $(src_dir)/*.cpp)
asm_sources := $(wildcard $(src_dir)/*.asm)

c_objects := $(subst src/,int/$(config)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(subst src/,int/$(config)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(subst src/,int/$(config)/, $(subst .asm,.asm.o, $(asm_sources)))

$(output_file): $(c_objects) $(cpp_objects) $(asm_objects)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -nostdlib -shared $(compile_flags) -o $@ $^ -e main -static -lgcc


$(int_dir)/%.c.o: $(src_dir)/%.c $(c_headers) $(cross_headers)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -ffreestanding -fno-stack-protector -fno-exceptions $(compile_defs) $(compile_flags) $(elf_include_flags) -c $< -o $@
$(int_dir)/%.cpp.o: $(src_dir)/%.cpp $(c_headers) $(cross_headers)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -ffreestanding -fno-stack-protector -fno-exceptions $(compile_defs) $(compile_flags) $(elf_include_flags) -c $< -o $@
$(int_dir)/%.asm.o: $(src_dir)/%.asm $(c_headers) $(cross_headers)
	@ mkdir -p $(dir $@)
	$(NASM) -g -f elf64 $(compile_defs) $(elf_nasm_include_flags) $< -o $@