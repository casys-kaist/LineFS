#!/bin/bash
sudo postfix stop
sudo pkill -9 local
sudo postsuper -d ALL 
sudo postfix start
sudo pkill -9 master && sudo /usr/libexec/postfix/master -w -d
