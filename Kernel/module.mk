localdir := Kernel

include_paths += 

C_HEADERS := $(wildcard $(localdir)/src/*.h)
CPP_SOURCES := $(wildcard $(localdir)/src/*.cpp)

CPP_OBJECTS := $(subst /src/,/obj/, $(subst .cpp,.o, $(CPP_SOURCES)))

kernel_filename := KERNEL.BIN

root_partition_img_deps += $(localdir)/bin/$(kernel_filename)

local_clean_files := $(addprefix $(localdir)/, obj bin)
clean_files += $(local_clean_files)

$(localdir)/bin/$(kernel_filename): $(CPP_OBJECTS)
	@ mkdir -p $(dir $@)
	ld -nostdlib $^ -o $@

$(localdir)/obj/%.o: $(localdir)/src/%.cpp $(C_HEADERS)
	@ mkdir -p $(dir $@)
	g++ -ffreestanding -c $< -o $@