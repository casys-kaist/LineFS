#! /bin/bash

# sudo cgdb -p `pgrep filebench | tail -n 1`
sudo gdb -p `pgrep filebench | tail -n 1`
