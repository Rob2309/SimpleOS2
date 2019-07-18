

$(bin_dir)/SimpleOS2.img: $(bin_dir)/partition.img
	@ dd if=/dev/zero of=$@ bs=512 count=110000 2>/dev/null
	@ parted $@ mklabel gpt mkpart SimpleOS2 fat32 2048s 104447s toggle 1 esp
	@ dd if=$< of=$@ bs=512 conv=notrunc seek=2048