localdir := Kernel

output_file_name := KERNEL.ELF

root_partition_img_deps += $(localdir)/bin/$(output_file_name)

include makeinclude/elfso.mk