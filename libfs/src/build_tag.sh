#! /bin/bash

rm -rf cscope.*
ctags -R . 

PWD=`pwd`\/;
find -L `pwd` \( -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.s' -o -name '*.S' -o -name '*.java' -o -name '*.jl' -o -name '*.py' \) -exec realpath {} \; -print > tmp.files
cat tmp.files | grep -v kernfs | uniq > cscope.files
rm -rf tmp.files
cscope -bR
