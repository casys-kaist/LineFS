#!/bin/bash

# scripts refers to
# http://docs.ceph.com/docs/jewel/install/manual-deployment/#monitor-bootstrapping

set -x

CEPH_DIR=/var/lib/ceph
PMEM_DEV=/dev/pmem0

#sudo umount /dev/pmem0
#TODO: error check

#sudo mkfs.btrfs $PMEM_DEV 
#sudo mkfs -t ext4 $PMEM_DEV 

CID="$(ceph osd create)"
mkdir -p $CEPH_DIR/osd/ceph-$CID
#sudo mount $PMEM_DEV $CEPH_DIR/osd/ceph-$CID
touch $CEPH_DIR/osd/ceph-$CID-journal
ceph-osd -i $CID --mkfs --mkkey --osd-data $CEPH_DIR/osd/ceph-$CID --osd-journal $CEPH_DIR/osd/ceph-$CID-journal
ceph auth add osd.$CID osd 'allow *' mon 'allow profile osd' -i $CEPH_DIR/osd/ceph-$CID/keyring

ceph osd crush add-bucket a host
ceph osd crush move a root=default
ceph osd crush add osd.$CID 1.0 host=a
ceph-osd -d -i $CID --osd-data $CEPH_DIR/osd/ceph-$CID --osd-journal $CEPH_DIR/osd/ceph-$CID-journal
