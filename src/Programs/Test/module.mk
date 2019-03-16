src_dir := src/Programs/Test

output_file := bin/$(config)/Programs/Test.elf

ramdisk_files += $(output_file)
ramdisk_command += $(output_file) test.elf

include makeinclude/elfso.mk