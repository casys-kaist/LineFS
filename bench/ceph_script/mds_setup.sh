#! /bin/bash

CLUSTER_NAME=mds
ID=a
mkdir -p /var/lib/ceph/mds/$CLUSTER_NAME.$ID

rm -rf /var/run/ceph/ceph-mds.a.asok
rm -rf /var/lib/ceph/mds/$CLUSTER_NAME.$ID/keyring

ceph-authtool --create-keyring /var/lib/ceph/mds/$CLUSTER_NAME.$ID/keyring --gen-key -n mds.$ID
ceph auth add mds.$ID osd "allow rwx" mds "allow" mon "allow profile mds" -i /var/lib/ceph/mds/$CLUSTER_NAME.$ID/keyring

ceph-mds -d --cluster $CLUSTER_NAME -i $ID -c /etc/ceph/ceph.conf
