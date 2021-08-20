#!/bin/bash

set -x

#sudo ./deploy.sh

CEPH_DIR=/var/lib/ceph
IP=192.168.15.2

#FSID must be identical to /etc/ceph/ceph.conf

FSID=78f02957-b07b-4c42-ae70-f9cdf1fffd2f
#TODO: FSID check

sudo ceph-authtool --create-keyring /tmp/ceph.mon.keyring --gen-key -n mon. --cap mon 'allow *'
sudo ceph-authtool --create-keyring /etc/ceph/ceph.client.admin.keyring --gen-key -n client.admin --set-uid=0 --cap mon 'allow *' --cap osd 'allow *' --cap mds 'allow'
sudo ceph-authtool /tmp/ceph.mon.keyring --import-keyring /etc/ceph/ceph.client.admin.keyring
sudo rm -rf /tmp/monmap
monmaptool --create --add sdp1 $IP --fsid $FSID /tmp/monmap
sudo mkdir -p $CEPH_DIR/mon/ceph-sdp1
sudo rm -rf $CEPH_DIR/mon/ceph-sdp1/*
sudo ceph-mon --cluster ceph  --mkfs -i sdp1 --monmap /tmp/monmap --keyring /tmp/ceph.mon.keyring --mon-data $CEPH_DIR/mon/ceph-sdp1
sudo touch $CEPH_DIR/mon/ceph-sdp1/done
#ceph-mon -i a --mon-data /var/lib/ceph/mon/ceph-a/ --keyring /tmp/ceph.mon.keyring
