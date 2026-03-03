source_files = $(wildcard *.cpp)
header_files = $(wildcard *.h)

keepassxc_harvester.so: $(source_files) $(header_files)
	g++ $(source_files) -shared -fPIC -ldl -o keepassxc_harvester.so