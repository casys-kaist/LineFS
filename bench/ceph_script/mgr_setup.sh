#!/bin/bash

set -x

#sudo ./deploy.sh

CEPH_DIR=/var/lib/ceph
IP=192.168.15.2
name=a

#FSID must be identical to /etc/ceph/ceph.conf

FSID=78f02957-b07b-4c42-ae70-f9cdf1fffd2f
#TODO: FSID check

#ceph auth get-or-create mgr.$name mon 'allow profile mgr' osd 'allow *' mds 'allow *'
ceph-mgr -i $name -d
