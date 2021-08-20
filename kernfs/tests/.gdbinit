#set debug auto-load on
define load_sl
    sharedlibrary libshim.so
end

#set environment LD_PRELOAD ../../shim/libshim/libshim.so
#set environment LD_PRELOAD /usr/local/lib/libmutrace.so	# mutrace
set environment LD_PRELOAD ../../libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2
set environment LD_LIBRARY_PATH ../build:../buildarm:../../libfs/lib/libspdk/libspdk/:../../libfs/lib/nvml/src/nondebug/:../../libfs/build/

# loading python modules.
source ../../gdb_python_modules/load_modules.py

define setbr
	l digest_directory
	b 396 if dirent_inum == 28
	l digest_replay_and_optimize
	b 946 if inum == 28
end

# this is macro to setup for breakpoint
define setup_br_tmp
    b 60
# run programe
    r w aa 40K

    l add_to_log
    b 541  if $caller_is("posix_write", 8)
    #b balloc if $caller_is("posix_write", 8)
end

define loghdrs_print
set $total = $arg0
set $i = 0
   while($i<$total)
     print $i
     print log_hdrs[$i]
     set $i = $i + 1
   end
end
