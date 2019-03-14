src_dir := src/RamdiskBuilder

output_file := tools/RamdiskBuilder.out

export RDBUILDER := $(output_file)

$(output_file): $(src_dir)/main.cpp
	@ mkdir -p $(dir $@)
	g++ -o $@ $^ -Isrc/common