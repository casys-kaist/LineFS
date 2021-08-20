#!/bin/bash
sudo /home/yulistic/bluefield/bluefield-smartnic-script/roce/setup_roce.sh
sudo /home/yulistic/bluefield/bluefield-smartnic-script/setup_iptables.sh
sudo ndctl destroy-namespace all -f
sudo utils/use_dax.sh bind
sudo kernfs/lib/spdk/scripts/setup.sh
# sudo mount kernfs
# sudo mount libfs
