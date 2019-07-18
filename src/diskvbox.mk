
disk_raw_rel := $(shell realpath --relative-to=. $(bin_dir)/SimpleOS2.img)
disk_vdi_rel := $(shell realpath --relative-to=. $(bin_dir)/SimpleOS2.vdi)

$(bin_dir)/SimpleOS2.vdi: $(bin_dir)/SimpleOS2.img
	@ rm -rf $@
	@ VBoxManage convertfromraw $(disk_raw_rel) $(disk_vdi_rel) --format VDI --uuid 430eee2a-0fdf-4d2a-88f0-5b99ea8cffca