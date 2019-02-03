localdir := Bootloader

output_file_name := BOOTX64.EFI

root_partition_img_deps += $(addprefix $(localdir)/, bin/$(output_file_name) res/config.cfg res/font.img)

include makeinclude/efimodule.mk