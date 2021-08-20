#! /usr/bin/python

# to load modules in current directory
# add "source /path/to/load_modules.py" to .gdbinit.

import glob
import os

# Search the python dir for all .py files, and source each
python_dir = os.path.dirname(__file__)
py_files = glob.glob("%s/*.py" % python_dir)
# to prevent recursive loading
py_files.remove(__file__)

for py_file in py_files:
    print(py_file)
    gdb.execute('source %s' % py_file)

print("loading python module is done")
