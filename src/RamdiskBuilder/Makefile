bin_dir := $(bin_dir)/tools

$(bin_dir)/RamdiskBuilder: main.cpp
	@ mkdir -p $(dir $@)
	g++ -o $@ $^ -I../common