localdir := Bootloader

include_paths := dep/gnu-efi/inc dep/gnu-efi/inc/x86_64

C_HEADERS := $(wildcard $(localdir)/*.h)
C_SOURCES := $(wildcard $(localdir)/*.c)
CPP_SOURCES := $(wildcard $(localdir)/*.cpp)
C_OBJECTS := ${C_SOURCES:.c=.o} ${CPP_SOURCES:.cpp=.o}

bootloader_filename := BOOTX64.EFI

local_partition_files := $(addprefix $(localdir)/, $(bootloader_filename) msg.txt)

root_partition_img_deps += ${local_partition_files}

clean_files += $(addprefix $(localdir)/, *.o *.EFI)



$(localdir)/$(bootloader_filename): ${C_OBJECTS}
	x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o $@ $^ -lgcc



$(localdir)/%.o: $(localdir)/%.c ${C_HEADERS}
	x86_64-w64-mingw32-gcc -ffreestanding $(addprefix -I, ${include_paths}) -c $< -o $@

$(localdir)/%.o: $(localdir)/%.cpp ${C_HEADERS}
	x86_64-w64-mingw32-gcc -ffreestanding $(addprefix -I, ${include_paths}) -c $< -o $@