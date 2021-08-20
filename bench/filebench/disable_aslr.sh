#!/bin/bash
# Disable ASLR. It is required to run filebench.
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
