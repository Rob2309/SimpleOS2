

elf_include_flags := $(addprefix -I, common)

c_headers := $(wildcard $(localdir)/src/*.h)

c_sources := $(wildcard $(localdir)/src/*.c)
cpp_sources := $(wildcard $(localdir)/src/*.cpp)
asm_sources := $(wildcard $(localdir)/src/*.asm)

c_objects := $(subst /src/,/obj/, $(subst .c,.o, $(c_sources)))
cpp_objects := $(subst /src/,/obj/, $(subst .cpp,.o, $(cpp_sources)))
asm_objects := $(subst /src/,/obj/, $(subst .asm,.o, $(asm_sources)))

output_file := $(localdir)/bin/$(output_file_name)

clean_files += $(localdir)/obj $(localdir)/bin



$(output_file): $(c_objects) $(cpp_objects) $(asm_objects)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -nostdlib -shared -o $@ $^ -e main -static -lgcc


$(localdir)/obj/%.o: $(localdir)/src/%.c $(c_headers)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -ffreestanding $(elf_include_flags) -c $< -o $@
$(localdir)/obj/%.o: $(localdir)/src/%.cpp $(c_headers)
	@ mkdir -p $(dir $@)
	$(ELF_GCC) -g -fPIC -ffreestanding $(elf_include_flags) -c $< -o $@
$(localdir)/obj/%.o: $(localdir)/src/%.asm
	@ mkdir -p $(dir $@)
	$(NASM) -g -f elf64 $< -o $@