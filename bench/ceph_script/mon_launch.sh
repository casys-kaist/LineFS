#!/bin/bash

CEPH_DIR=/var/lib/ceph

sudo ceph-mon -i sdp1 --mon-data $CEPH_DIR/mon/ceph-sdp1/ --keyring /tmp/ceph.mon.keyring -d
