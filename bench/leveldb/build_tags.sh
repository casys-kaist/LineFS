#! /bin/bash

ctags --c++-kinds=+p --fields=+iaS --extra=+q --language-force=C++
mk_cscope
cscope -bR
