src_dir := src/Kernel

output_file := bin/Kernel.sys

root_partition_img_deps += $(output_file)

include makeinclude/elfso.mk