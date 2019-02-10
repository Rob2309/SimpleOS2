localdir := Programs/Test

output_file_name := Test.elf

root_partition_img_deps += $(localdir)/bin/$(output_file_name)

include makeinclude/elfso.mk