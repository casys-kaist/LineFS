#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import re

## USE: python parsing_lat.py /results/dir

#  directory = os.getcwd()  # use current dir path.

if len(sys.argv)<2:
    print "Usage: python parsing_lat.py /results/dir"
    sys.exit(1)

dir_path = sys.argv[1]
avg_val = {}
min_val = {}
max_val = {}
std_val = {}
p50_val = {}
p99_val = {}
p999_val = {}
p9999_val = {}
p99999_val = {}
fsync_val = {}
#io_sizes = ['128B', '1K', '4K', '8K', '16K', '32K', '64K', '128K', '256K', '512K', '1M']
io_sizes = ['128B', '1K', '4K', '16K', '64K', '1M']

for filename in os.listdir(dir_path):
    if filename == "output.txt" or filename == "output_paper.txt":
        continue
    io_size = filename.strip().split("_")[2]
    #print filename.strip().split("_")
    result_file = open(os.path.join(dir_path, filename))
    for line in result_file:
        if "fsync-avg" in line:
            fsync_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
        elif "avg:" in line:
            avg_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
        elif "min:" in line:
            min_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
        elif "max:" in line:
            max_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
        elif "std:" in line:
            std_val[io_size] = re.split('\s|\t|\n|\)|\(',line.strip())[4].strip()
        elif "99.999 percentile" in line:
            p99999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
        elif "99.99 percentile" in line:
            p9999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
        elif "99.9 percentile" in line:
            p999_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
        elif "99 percentile" in line:
            p99_val[io_size] = re.split('\(|usec',line.strip())[1].strip()
        elif "50 percentile" in line:
            p50_val[io_size] = re.split('\(|usec',line.strip())[1].strip()

out_file = open (os.path.join(dir_path, "output.txt"), "w")

# Print headers.
if fsync_val:
    # Print all values.
    # out_file.write("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" % ("io_size", "avg", "min", "max", "std", "p50", "p99", "p99.9", "p99.99", "p99.999", "fsync-avg"))
    # print ("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s" % ("io_size", "avg", "min", "max", "std", "p50", "p99", "p99.9", "p99.99", "p99.999", "fsync_avg"))

    # Print values reported to LineFS paper.
    out_file.write("%s,%s,%s,%s,%s\n" % ("io_size", "avg", "p99", "p99.9", "fsync-avg"))
    # print ("%s,%s,%s,%s,%s" % ("io_size", "avg", "p99", "p99.9", "fsync_avg"))
else:
    out_file.write("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" % ("io_size", "avg", "min", "max", "std", "p50", "p99", "p99.9", "p99.99", "p99.999"))
    print ("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s" % ("io_size", "avg", "min", "max", "std", "p50", "p99", "p99.9", "p99.99", "p99.999"))

# Print numbers.
for i in io_sizes:
    if avg_val.has_key(i):
        if fsync_val:
            # Print all values.
            # out_file.write("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i], p999_val[i], p9999_val[i], p99999_val[i], fsync_val[i]))
            # print ("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i], p999_val[i], p9999_val[i], p99999_val[i], fsync_val[i]))

            # Print values reported to LineFS paper.
            out_file.write("%s,%s,%s,%s,%s\n" % (i, avg_val[i], p99_val[i], p999_val[i], fsync_val[i]))
            # print ("%s,%s,%s,%s,%s" % (i, avg_val[i], p99_val[i], p999_val[i], fsync_val[i]))
        else:
            out_file.write("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i], p999_val[i], p9999_val[i], p99999_val[i]))
            print ("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i], p999_val[i], p9999_val[i], p99999_val[i]))
out_file.close()

# print format for paper.
# out_paper_file = open (os.path.join(dir_path, "output_paper.txt"), "w")
# for i in io_sizes:
#     if avg_val.has_key(i):
#         if fsync_val:
#             out_paper_file.write ("%s,1,ASYNC,%s,%s,%s,%s,%s,%s,%s\n" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i], fsync_val[i]))
#         else:
#             out_paper_file.write ("%s,1,ASYNC,%s,%s,%s,%s,%s,%s\n" % (i, avg_val[i], min_val[i], max_val[i], std_val[i], p50_val[i], p99_val[i]))
# out_paper_file.close()
