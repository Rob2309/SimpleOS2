include_flags := $(addprefix -I,. $(includes))

c_headers := $(shell find . -type f -name "*.h" -printf "%p ") $(foreach addh,$(includes),$(shell find $(addh) -type f -name "*.h" -printf "%p "))
cross_headers := $(shell find . -type f -name "*.inc" -printf "%p ") $(foreach addh,$(includes),$(shell find $(addh) -type f -name "*.inc" -printf "%p "))

c_sources := $(shell find . -type f -name "*.c" -printf "%p ")
cpp_sources := $(shell find . -type f -name "*.cpp" -printf "%p ")
asm_sources := $(shell find . -type f -name "*.asm" -printf "%p ")

c_objects := $(addprefix $(int_dir)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(addprefix $(int_dir)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(addprefix $(int_dir)/, $(subst .asm,.asm.o, $(asm_sources)))

$(bin_dir)/$(out_file): $(c_objects) $(cpp_objects) $(asm_objects)
	@ echo "\e[33mLinking executable\e[0m"
	@ mkdir -p $(dir $@)
	@ $(cpp_compiler) $(link_flags) -o $@ $^

$(int_dir)/%.c.o: ./%.c $(c_headers) $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(cpp_compiler) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.cpp.o: ./%.cpp $(c_headers) $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(cpp_compiler) $(compile_flags) $(include_flags) -c $< -o $@
$(int_dir)/%.asm.o: ./%.asm $(cross_headers)
	@ echo "\e[33mCompiling $<\e[0m"
	@ mkdir -p $(dir $@)
	@ $(asm_compiler) $< -o $@