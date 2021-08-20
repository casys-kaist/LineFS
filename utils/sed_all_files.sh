#! /bin/bash

find . -type f -name "*.[c|h]"  -print0 | xargs -0 sed -i "s/$1/$2/g"
