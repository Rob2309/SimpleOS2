int_dir := $(subst src/,int/$(config)/, $(src_dir))

efi_include_flags := $(addprefix -I, dep/gnu-efi/inc dep/gnu-efi/inc/x86_64 src/common)

c_headers := $(wildcard $(src_dir)/*.h) $(wildcard src/common/*.h)

c_sources := $(wildcard $(src_dir)/*.c)
cpp_sources := $(wildcard $(src_dir)/*.cpp)

c_objects := $(subst src/,int/$(config)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(subst src/,int/$(config)/, $(subst .cpp,.cpp.o, $(cpp_sources)))


$(output_file): $(c_objects) $(cpp_objects)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc


$(int_dir)/%.c.o: $(src_dir)/%.c $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(compile_defs) $(efi_include_flags) -c $< -o $@
$(int_dir)/%.cpp.o: $(src_dir)/%.cpp $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(compile_defs) $(efi_include_flags) -c $< -o $@