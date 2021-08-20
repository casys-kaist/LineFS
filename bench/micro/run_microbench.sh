#! /bin/bash

./mk_pmem.sh
./microbench.sh | tee m_ext4_dax.txt
sudo umount ./pmem

./mk_f2fs.sh
./microbench_f2fs.sh | tee m_f2fs.txt
sudo umount ./ssd

./microbench_pmfs.sh | tee m_pmfs.txt
