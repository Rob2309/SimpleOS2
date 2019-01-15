
include Bootloader/module.mk

clean_files += *.img

run: partition.img FORCE
	qemu-system-x86_64 -cpu qemu64 -net none -L /usr/share/ovmf/ -bios OVMF.fd -drive file=$<,if=ide



partition.img: ${root_partition_img_deps}
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mcopy -i $@ $^ ::

clean: FORCE
	rm -rf ${clean_files}

FORCE: 