#!/bin/bash
sudo stress-ng -c 16 -l 100 --taskset 0-15
# sudo stress-ng -c 1 -l 100 --taskset 0-15
