#!/bin/bash

CEPH_DIR=/var/lib/ceph

set -x

#sudo rm -f /etc/ceph/ceph.conf
sudo rm -f /tmp/ceph.mon.keyring
sudo rm -f /etc/ceph/ceph.client.admin.keyring
sudo rm -f /tmp/ceph.mon.keyring
sudo rm -rf $CEPH_DIR/mon/ceph-sdp1
sudo rm -f /tmp/monmap
