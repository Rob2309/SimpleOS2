localdir := Bootloader

C_SOURCES := $(wildcard $(localdir)/*.c)
C_OBJECTS := ${C_SOURCES:.c=.o}

local_partition_files := $(localdir)/BOOTX64.EFI $(localdir)/msg.txt
partition_files += ${local_partition_files}

clean_files += $(localdir)/*.o $(localdir)/*.EFI

$(localdir)/BOOTX64.EFI: ${C_OBJECTS}
	x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $< -lgcc

$(localdir)/%.o: $(localdir)/%.c
	x86_64-w64-mingw32-gcc -ffreestanding -I/usr/include/efi -I/usr/include/efi/x86_64 -c $< -o $@