localdir := Kernel

output_file_name := Kernel.sys

root_partition_img_deps += $(localdir)/bin/$(output_file_name)

ramdisk_files += $(localdir)/res/font.fnt
ramdisk_command += $(localdir)/res/font.fnt font.fnt

include makeinclude/elfso.mk