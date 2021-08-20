from __future__ import division
import os, sys
import re
import math
import pprint

def convert_size(size):
   if (size == 0):
       return '0B'
   size_name = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
   i = int(math.floor(math.log(size,1024)))
   p = math.pow(1024,i)
   s = round(size/p,2)
   return "{} {}".format(int(s),size_name[i])

def to_bytes(s):
   if s[-1] == 'G':
      return float(s[:-1])*1024*1024*1024
   if s[-1] == 'M':
      return float(s[:-1])*1024*1024
   if s[-1] == 'K':
      return float(s[:-1])*1024


def size_cmp(s1, s2):
   if to_bytes(s1) > to_bytes(s2):
      return -1;
   else:
      return 1;


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Wrong number of arguments, must pass the relative path to the directory with the results of the benchmark"
        quit()

    filenames = os.listdir(sys.argv[1])

    data = {}
    fs_types = []
    io_sizes  = []
    total_sizes = []
    io_patterns = []

    for filename in filenames:
        split = filename.split('.')[0].split('_')

        fs_types.append(split[0])
	io_patterns.append(split[1])
	total_sizes.append(split[2])
	io_sizes.append(split[3])

    fs_types = list(set(fs_types))
    fs_types.sort()
    
    io_patterns = list(set(io_patterns))
    io_patterns.sort()

    total_sizes = list(set(total_sizes))
    total_sizes.sort()

    io_sizes = list(set(io_sizes))
    io_sizes.sort(cmp=size_cmp, reverse=True)

    for total_size in total_sizes:
        data[total_size] = {}
        for io_pattern in io_patterns:
            data[total_size][io_pattern] = {}
            for io_size in io_sizes:
               data[total_size][io_pattern][io_size] = {}
               for fs_type in fs_types:
                  data[total_size][io_pattern][io_size][fs_type] = {}
                  data[total_size][io_pattern][io_size][fs_type] = {'time': 0, 'throughput': 0}

    for filename in filenames:
	split = filename.split('.')[0].split('_')
        fs_type = split[0]
	io_pattern = split[1]
	total_size = split[2]
	io_size = split[3]

        with open(os.path.join(sys.argv[1], filename)) as fp:
	   f_content = fp.read()
           time = re.search("avg: (.*)", f_content)
	   tput = re.search("Throughput: (.*) MB", f_content)
           std = re.search("std: (.*)", f_content)

           if time is not None:
              data[total_size][io_pattern][io_size][fs_type]['time'] = float(time.group(1))
              data[total_size][io_pattern][io_size][fs_type]['time_out'] = "{:.3f}s ({:.3f})".format(
                 float(time.group(1)), float(std.group(1)))
           else:
              data[total_size][io_pattern][io_size][fs_type]['time'] = "ERR"
              
           data[total_size][io_pattern][io_size][fs_type]['throughput'] = float(tput.group(1)) if time is not None else "ERR"

    pp = pprint.PrettyPrinter(indent=4) 
    pp.pprint(data)
    #at this point we have all the data in the nested dict
    #but since I used set to remove duplicates, we need to sort again


    #col_count_alignment = 'r' + (('rr'*len(fs_types)*(len(io_patterns))+'|') * len(total_sizes))
    col_count_alignment = 'r' + (('rrrrr'*(len(io_patterns))+'|') * len(total_sizes)) 
    col_count_alignment = col_count_alignment[:-1]


    #depth_pat_clines = ''
    #for i in range(1, len(q_depths) * len(io_patterns)*3, 12):
    #    depth_pat_clines += '\\cmidrule(lr){{{}-{}}} '.format(i+2, i+13)
    #depth_pat_clines += '\n\n'

    lv1_multicolumn = '& '
    for total_size in total_sizes:
    	lv1_multicolumn += ' \multicolumn{{{}}}{{c}}{{{}}} &'.format(len(io_patterns)*(len(fs_types)*2+1), total_size)
    lv1_multicolumn = lv1_multicolumn[:-1] + "\\\\\n"

    
    lv2_multicolumn = ''
    for io_pattern in io_patterns:
       lv2_multicolumn += ' \multicolumn{{{}}}{{c}}{{{}}} &'.format(len(fs_types)*2+1, io_pattern)
    #lv2_multicolumn += '&';
    
    lv2_multicolumn = '& '+ lv2_multicolumn*len(total_sizes)
    lv2_multicolumn = lv2_multicolumn[:-1] + "\\\\\n"

    lv2_clines = ''
    for i in range(1,len(total_sizes)*len(io_patterns)*len(fs_types)*2+1, len(fs_types)*2+1):
       lv2_clines += '\\cmidrule(lr){{{}-{}}} '.format(i+1, i+1+2*len(fs_types))
    lv2_clines += '\n\n'

    
    lv3_multicolumn = ''
    for fs_type in fs_types:
       lv3_multicolumn += ' \multicolumn{{{}}}{{c}}{{{}}} & '.format(2, fs_type)
    lv3_multicolumn += '&'   
    lv3_multicolumn = '&' + lv3_multicolumn*len(total_sizes)*len(io_patterns)
    lv3_multicolumn = lv3_multicolumn[:-1] + "\\\\\n\n"
   
    
    lv3_clines = ''
    for i in range(1,len(total_sizes)*len(io_patterns)*len(fs_types)*2+1, 5):
       lv3_clines += '\\cmidrule(lr){{{}-{}}} \\cmidrule(lr){{{}-{}}}'.format(i+1, i+2, i+3, i+4)
    lv3_clines += '\n\n'
    
    
    #io_pat_clines = ''
    #for i in range(1, len(q_depths) * len(io_patterns)*3, 3):
    #    io_pat_clines += '\\cmidrule(lr){{{}-{}}} '.format(i+2, i+4)
    #io_pat_clines += '\n\n'

    cols = ' Time & Throughput & Time & Throughput & Speedup &' * (len(total_sizes) * len(io_patterns))


    tex_output = '\\begin{{tabular}}{{{}}}\n'.format(col_count_alignment)
    tex_output += '\\toprule\n'
    tex_output += lv1_multicolumn
    tex_output += lv2_multicolumn
    tex_output += lv2_clines                                                   
    tex_output += lv3_multicolumn
    tex_output += lv3_clines
    tex_output += ' IO size  & ' + cols[:-2] + '\\\\ \n\n'


    for io_size in io_sizes:
       tex_output += "{} &".format(io_size)
       for total_size in total_sizes:
          for io_pattern in io_patterns:
             last = None
             for fs_type in fs_types:            
                tex_output += "{} & {:.0f} MB/s & ".format(
		    data[total_size][io_pattern][io_size][fs_type]['time_out'],
		    data[total_size][io_pattern][io_size][fs_type]['throughput']
                )
                if last is not None:
                   tex_output += "{:.1f} & ".format(
                      data[total_size][io_pattern][io_size][last]['time']
                      / data[total_size][io_pattern][io_size][fs_type]['time']
                   )
                last = fs_type


                
		#remove trailing ampersand
       tex_output = tex_output[:-2];
       tex_output += '\\\\'
       tex_output += '\hline \\\\ \n' if total_size is not total_sizes[-1] else ''

    tex_output += '\n\\end{tabular}'

    #last minute replaces:
    tex_output.replace("sw", "Sequential Write")
    #tex_output.replace("sr", "Sequential Read")
    tex_output.replace("rw", "Random Write")
    #tex_output.replace("rr", "Sequential Write")

    
    print tex_output
        #mag = 0, total_size_t = total_size
        #while total_size_t > 1024:
        #    io_size_t / =1024, mag += 1
