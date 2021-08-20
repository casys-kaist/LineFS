# Download PARSEC3.0-core and build it.
# gcc 7.5.0 was used.

.PHONY: redownload all
all:
	(cd parsec-3.0; bin/parsecmgmt -a build -p streamcluster)

redownload:
	@rm -rf parsec-3.0
	wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-core.tar.gz
	tar -xzf parsec-3.0-core.tar.gz
	@rm -rf parsec-3.0-core.tar.gz
