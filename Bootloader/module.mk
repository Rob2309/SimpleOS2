localdir := Bootloader

include_paths += $(addprefix -I, dep/gnu-efi/inc dep/gnu-efi/inc/x86_64)

C_HEADERS := $(wildcard $(localdir)/src/*.h)
C_SOURCES := $(wildcard $(localdir)/src/*.c)

C_OBJECTS := $(subst /src/,/obj/, $(subst .c,.o, $(C_SOURCES)))

bootloader_filename := BOOTX64.EFI

root_partition_img_deps += $(addprefix $(localdir)/, bin/$(bootloader_filename) res/msg.txt)

local_clean_files := $(addprefix $(localdir)/, obj bin)
clean_files += $(local_clean_files)



${localdir}/bin/$(bootloader_filename): ${C_OBJECTS}
	@ mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc



${localdir}/obj/%.o: $(localdir)/src/%.c $(C_HEADERS)
	@ mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc -ffreestanding $(include_paths) -c $< -o $@