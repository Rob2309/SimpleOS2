localdir := Programs/Test

output_file_name := Test.elf

ramdisk_files += $(localdir)/bin/$(output_file_name)
ramdisk_command += $(localdir)/bin/$(output_file_name) test.elf

include makeinclude/elfso.mk