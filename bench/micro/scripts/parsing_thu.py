#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import re

## USE: python jy_parsing.py /results/dir

#  directory = os.getcwd()  # use current dir path.

if len(sys.argv)<2:
    print "Usage: python jy_parsing_trh2.py /results/dir"
    sys.exit(1)

dir_path = sys.argv[1]
thru_val = {}
#avg_val = {}
#min_val = {}
#max_val = {}
#std_val = {}
#p99_val = {}
#p999_val = {}
#p9999_val = {}
#p99999_val = {}
#io_sizes = ['128B', '1K', '4K', '8K', '16K', '32K', '64K', '128K', '256K', '512K', '1M']
io_sizes = ['128B', '1K', '4K', '16K', '64K', '1M']

for filename in os.listdir(dir_path):
    if filename == "output.txt":
        continue
    io_size = filename.strip().split("_")[2]
    #print filename.strip().split("_")
    result_file = open(os.path.join(dir_path, filename))
    for line in result_file:
        if "throughput" in line:
            thru_val[io_size] = filter(None,re.split('\s|:|MB',line.strip()))[2].strip()

#        if "avg:" in line:
#            avg_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
#        elif "min:" in line:
#            min_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
#        elif "max:" in line:
#            max_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
#        elif "std:" in line:
#            std_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
#        elif "99.999 percentile" in line:
#            p99999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
#        elif "99.99 percentile" in line:
#            p9999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
#        elif "99.9 percentile" in line:
#            p999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
#        elif "99 percentile" in line:
#            p99_val[io_size] = re.split('\(|usec',line.strip())[1].strip()

print (thru_val)

out_file = open (os.path.join(dir_path, "output.txt"), "w")

out_file.write("%s,%s\n" % ("io_size", "throughput(MB/s)"))
print ("%s,%s" % ("io_size", "throughput"))
for i in io_sizes:
    if thru_val.has_key(i):
        out_file.write("%s,%s\n" % (i, thru_val[i]))
        print ("%s,%s" % (i, thru_val[i]))
