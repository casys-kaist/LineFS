import subprocess
import random
import math
import re

total_lbas = 62514774
#total_lbas = 2**20
#lba_offset = 2**16
lba_offset = 0
block_size = 4096
mb = 1024*1024

##
## Modify these
##
full_writes = 4
tput_lbas = int(total_lbas/4)


for io_size_mbs in [8, 64, 128, 256, 512, 1024]:
    for utilization_range in [0.1, 0.25, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]:
#for i in range(0,1):
#    for utilization_range, io_size_mbs in [(0.5, 64), (0.75, 64), (0.5, 8), (0.75, 8), (1, 64)]:
        io_size = io_size_mbs*mb
        precond_lbas = int(total_lbas*full_writes)
        io_size_lbas = io_size/block_size

        tp_sum = 0.0
        tp_total = 0
        first = True
            
        subprocess.call("blkdiscard /dev/nvme0n1", shell=True)
        print("About to run {} io_size'd writes before measuring throughput with {} writes. IO size is {}, utilization is {}".format(precond_lbas/io_size_lbas, 
            tput_lbas/io_size_lbas, io_size_mbs, utilization_range))

        for i in range(0, precond_lbas+tput_lbas, io_size_lbas):
            aligned = (math.floor(total_lbas*utilization_range) / io_size_lbas) -1
            seek = lba_offset + (int(math.floor(random.random() * aligned)) * io_size_lbas)
            #assert (random.random() * aligned * io_size_lbas)*block_size % io_size == 0  
            cmd = "dd if=/dev/zero of=/dev/nvme0n1 bs=4k count={} seek={} conv=fdatasync".format(io_size_lbas, seek)
            out = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
            if first:
                one = float(re.search("copied, (.*) s,", out).group(1))
                print("The first writes should take around {} mins and throughput should take {} mins".format(
                    ((precond_lbas/io_size_lbas)*one)/60, 
                    ((tput_lbas/io_size_lbas)*one)/60))
                first = False

            if i > (total_lbas*full_writes):
                res = re.search("\d* s, (\d+\.?\d*) MB/s", out)
                if res is None:
                    print("ERR: " + out)
                    continue
                tp_sum += float(res.group(1))
                tp_total += 1


        print("****\nio {} util {} throughput: {} MB/s\n*******".format(io_size_mbs, utilization_range, tp_sum/tp_total))
