
rdbuilder := $(bin_dir)/tools/RamdiskBuilder

rdfiles := $(bin_dir)/Programs/Init/init.elf $(bin_dir)/Programs/Test2/test2.elf
rdcmd := $(bin_dir)/Programs/Init/init.elf Init.elf $(bin_dir)/Programs/Test2/test2.elf Test2.elf

$(bin_dir)/initrd: $(rdbuilder) $(rdfiles)
	@ $(rdbuilder) $@ $(rdcmd)