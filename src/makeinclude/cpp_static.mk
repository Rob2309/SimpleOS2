# This makefile is used to create any C/C++/asm static library

include_flags := $(addprefix -I,. $(includes))

cross_headers := $(shell find . -type f -name "*.inc" -printf "%p ") $(foreach addh,$(includes),$(shell find $(addh) -type f -name "*.inc" -printf "%p "))

c_sources := $(shell find . -type f -name "*.c" -printf "%p ")
cpp_sources := $(shell find . -type f -name "*.cpp" -printf "%p ")
asm_sources := $(shell find . -type f -name "*.asm" -printf "%p ")

c_objects := $(addprefix $(int_dir)/, $(subst .c,.c.o, $(c_sources)))
cpp_objects := $(addprefix $(int_dir)/, $(subst .cpp,.cpp.o, $(cpp_sources)))
asm_objects := $(addprefix $(int_dir)/, $(subst .asm,.asm.o, $(asm_sources)))

$(bin_dir)/$(out_file): $(c_objects) $(cpp_objects) $(asm_objects)
	@ printf "\e[33mCreating static lib\e[0m\n"
	@ mkdir -p $(dir $@)
	@ ar r $@ $^ 2>/dev/null

$(int_dir)/%.c.o: ./%.c
	@ printf "\e[33mCompiling $<\e[0m\n"
	@ mkdir -p $(dir $@)
	@ $(cpp_compiler) $(compile_flags) $(include_flags) -c $< -o $@ -MMD -MP -MF $(@:.c.o=.c.d)
$(int_dir)/%.cpp.o: ./%.cpp
	@ printf "\e[33mCompiling $<\e[0m\n"
	@ mkdir -p $(dir $@)
	@ $(cpp_compiler) $(compile_flags) $(include_flags) -std=c++17 -c $< -o $@ -MMD -MP -MF $(@:.cpp.o=.cpp.d)
$(int_dir)/%.asm.o: ./%.asm $(cross_headers)
	@ printf "\e[33mCompiling $<\e[0m\n"
	@ mkdir -p $(dir $@)
	@ $(asm_compiler) $< -o $@

-include $(c_objects:.c.o=.c.d)
-include $(cpp_objects:.cpp.o=.cpp.d)