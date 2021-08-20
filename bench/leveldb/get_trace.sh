#! /bin/bash

rm -rf a.trace
grep -v 'write(1' f.trace > a.trace
