localdir := RamdiskBuilder

output_file_name := RamdiskBuilder.out

export RDBUILDER := $(localdir)/bin/$(output_file_name)

$(localdir)/bin/$(output_file_name): $(localdir)/src/main.cpp
	@ mkdir -p $(dir $@)
	g++ -o $@ $^ -Icommon

clean_files += $(localdir)/bin