src_dir := src/Bootloader

output_file := bin/BOOTX64.EFI

root_partition_img_deps += $(output_file)

include makeinclude/efimodule.mk