localdir := Bootloader

include_paths := dep/gnu-efi/inc dep/gnu-efi/inc/x86_64

C_HEADERS := $(wildcard $(localdir)/src/*.h)
C_SOURCES := $(wildcard $(localdir)/src/*.c)

C_OBJECTS := $(subst /src/,/obj/, $(subst .c,.o, $(C_SOURCES)))

bootloader_filename := BOOTX64.EFI

local_partition_files := $(addprefix $(localdir)/, bin/$(bootloader_filename) res/msg.txt)

root_partition_img_deps += ${local_partition_files}

clean_files += $(addprefix $(localdir)/, obj bin)



$(localdir)/bin/$(bootloader_filename): ${C_OBJECTS}
	@ mkdir -p $(localdir)/bin
	x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc



$(localdir)/obj/%.o: $(localdir)/src/%.c $(C_HEADERS)
	@ mkdir -p $(localdir)/obj
	x86_64-w64-mingw32-gcc -ffreestanding $(addprefix -I, $(include_paths)) -c $< -o $@