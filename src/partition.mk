
$(bin_dir)/partition.img: $(bin_dir)/Kernel/Kernel.sys $(bin_dir)/Bootloader/BOOTX64.EFI $(bin_dir)/initrd
	@ dd if=/dev/zero of=$@ bs=512 count=102400 2>/dev/null
	@ mkfs.fat $@ >/dev/null
	@ $(MTOOLS_MMD) -i $@ ::/EFI
	@ $(MTOOLS_MMD) -i $@ ::/EFI/BOOT
	@ $(MTOOLS_MCOPY) -i $@ $^ ::/EFI/BOOT