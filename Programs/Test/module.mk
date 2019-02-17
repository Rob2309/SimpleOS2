localdir := Programs/Test

output_file_name := ramdisk.img

root_partition_img_deps += $(localdir)/bin/$(output_file_name)

include makeinclude/elfso.mk