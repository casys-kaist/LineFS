#!/bin/bash

set -x

#sudo ./deploy.sh

CEPH_DIR=/var/lib/ceph
IP=192.168.15.2

#FSID must be identical to /etc/ceph/ceph.conf

FSID=78f02957-b07b-4c42-ae70-f9cdf1fffd2f
#TODO: FSID check

#ceph osd pool create cephfs_data 4420
#ceph osd pool create cephfs_metadata 1840


ceph osd pool create cephfs_data 1
ceph osd pool create cephfs_metadata 1

ceph fs new cephfs cephfs_metadata cephfs_data

