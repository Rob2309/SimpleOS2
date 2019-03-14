int_dir := $(subst src/,int/, $(src_dir))

efi_include_flags := $(addprefix -I, dep/gnu-efi/inc dep/gnu-efi/inc/x86_64 src/common)

c_headers := $(wildcard $(src_dir)/*.h) $(wildcard src/common/*.h)

c_sources := $(wildcard $(src_dir)/*.c)
cpp_sources := $(wildcard $(src_dir)/*.cpp)

c_objects := $(subst src/,int/, $(subst .c,.o, $(c_sources)))
cpp_objects := $(subst src/,int/, $(subst .cpp,.o, $(cpp_sources)))


$(output_file): $(c_objects) $(cpp_objects)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc


$(int_dir)/%.o: $(src_dir)/%.c $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(efi_include_flags) -c $< -o $@
$(int_dir)/%.o: $(src_dir)/%.cpp $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(efi_include_flags) -c $< -o $@