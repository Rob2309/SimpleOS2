

efi_include_flags := $(addprefix -I, dep/gnu-efi/inc dep/gnu-efi/inc/x86_64)

c_headers := $(wildcard $(localdir)/src/*.h)

c_sources := $(wildcard $(localdir)/src/*.c)
cpp_sources := $(wildcard $(localdir)/src/*.cpp)

c_objects := $(subst /src/,/obj/, $(subst .c,.o, $(c_sources)))
cpp_objects := $(subst /src/,/obj/, $(subst .cpp,.o, $(cpp_sources)))

output_file := $(localdir)/bin/$(output_file_name)

clean_files += $(localdir)/obj $(localdir)/bin



$(output_file): $(c_objects) $(cpp_objects)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc


$(localdir)/obj/%.o: $(localdir)/src/%.c $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(efi_include_flags) -c $< -o $@
$(localdir)/obj/%.o: $(localdir)/src/%.cpp $(c_headers)
	@ mkdir -p $(dir $@)
	$(PE_GCC) -ffreestanding $(efi_include_flags) -c $< -o $@