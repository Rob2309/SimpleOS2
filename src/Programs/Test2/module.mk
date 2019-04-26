src_dir := src/Programs/Test2

output_file := bin/$(config)/Programs/Test2.elf

ramdisk_files += $(output_file)
ramdisk_command += $(output_file) test2.elf

include makeinclude/elfexe.mk