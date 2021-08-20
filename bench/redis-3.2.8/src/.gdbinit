define load_sl
	sharedlibrary libshim.so
	sharedlibrary libmlfs.so
end

#set follow-fork-mode child
set follow-fork-mode parent
set follow-exec-mode new
set detach-on-fork off

#set auto-solib-add on
set auto-load safe-path ../../../shim/glibc-build:../../../shim/glibc-2.19
set libthread-db-search-path ../../../shim/glibc-build/nptl_db/
set environment LD_PRELOAD ../../../shim/libshim/libshim.so
set environment LD_LIBRARY_PATH ../../../libfs/lib/nvml/src/nondebug/:../../../libfs/build/:../../../shim/glibc-build/rt/:/usr/local/lib/:/usr/lib/x86_64-linux-gnu/:/lib/x86_64-linux-gnu/
set environment MLFS 1

# loading python modules.
source ../../../gdb_python_modules/load_modules.py

define do_run
r ../redis_mlfs.conf
end

define sigcont
signal SIGCONT
end

define do_normal_run
end

# this is macro to setup for breakpoint
define setup_br
	b 60
# run programe
	r w aa 40K

	l add_to_log
	b 541  if $caller_is("posix_write", 8)
	#b balloc if $caller_is("posix_write", 8)
end
