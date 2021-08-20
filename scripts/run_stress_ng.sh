#!/bin/bash
# CPU test.
sudo stress-ng -c 16 -l 100 --taskset 0-15

# Memory test.
# sudo stress-ng -m 16 --taskset 0-15
