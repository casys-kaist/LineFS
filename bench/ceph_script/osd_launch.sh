#!/bin/bash

# scripts refers to
# http://docs.ceph.com/docs/jewel/install/manual-deployment/#monitor-bootstrapping

set -x

CEPH_DIR=/var/lib/ceph
PMEM_DEV=/dev/pmem0
CID=0

ceph-osd -d -i $CID --osd-data $CEPH_DIR/osd/ceph-$CID --osd-journal $CEPH_DIR/osd/ceph-$CID-journal
