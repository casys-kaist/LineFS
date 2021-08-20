#!/bin/bash
sudo mkdir -p /etc/ceph
sudo mkdir -p /var/lib/ceph

sudo cat > /etc/ceph/ceph.conf <<'endmsg'
[global]
fsid = c25fadc1-b648-4bd5-a7e2-e8d141bfe84f
mon_initial_members = sdp1
mon_host = 192.168.15.2
auth_cluster_required = none
auth_service_required = none
auth_client_required = none

mon allow pool delete = true
osd pool default size = 1
osd pool default min size = 1
[osd]
        osd journal size = 1024

[osd.0]
        host = node1
endmsg
