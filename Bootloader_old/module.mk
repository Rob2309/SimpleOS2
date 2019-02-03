localdir := Bootloader

output_file_name := BOOTX64.EFI

root_partition_img_deps += $(addprefix $(localdir)/, bin/$(output_file_name) res/msg.txt res/font.bin)

include makeinclude/efimodule.mk