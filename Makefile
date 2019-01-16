include Bootloader/module.mk

clean_files += *.img

run: partition.img FORCE
	qemu-system-x86_64 -cpu qemu64 -net none -drive if=pflash,unit=0,format=raw,file=dep/ovmf/x64/OVMF_CODE.fd,readonly=on -drive if=pflash,unit=1,format=raw,file=dep/ovmf/x64/OVMF_VARS.fd -drive file=$<,if=ide



partition.img: ${root_partition_img_deps}
	dd if=/dev/zero of=$@ bs=512 count=102400
	mkfs.fat $@
	mcopy -i $@ $^ ::

clean: FORCE
	rm -rf ${clean_files}

FORCE: 