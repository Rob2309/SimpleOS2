localdir := Kernel

output_file_name := Kernel.sys

root_partition_img_deps += $(localdir)/bin/$(output_file_name)

include makeinclude/elfso.mk