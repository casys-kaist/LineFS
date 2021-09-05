#!/bin/bash
# This scirpts formats device (mkfs) and runs kernfs.
if [ "$(gcc -dumpmachine)" != "aarch64-linux-gnu" ]; then
    make mkfs
fi
