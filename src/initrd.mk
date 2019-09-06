
rdbuilder := $(bin_dir)/tools/RamdiskBuilder

rdfiles := $(bin_dir)/Programs/Init/init.elf $(bin_dir)/Programs/TestVFS/testvfs.elf $(bin_dir)/Programs/Shell/shell.elf
rdcmd := $(bin_dir)/Programs/Init/init.elf Init.elf $(bin_dir)/Programs/TestVFS/testvfs.elf TestVFS.elf $(bin_dir)/Programs/Shell/shell.elf Shell.elf

$(bin_dir)/initrd: $(rdbuilder) $(rdfiles)
	@ $(rdbuilder) $@ $(rdcmd)