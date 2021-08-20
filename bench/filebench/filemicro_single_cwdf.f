#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

set $dir=/mlfs
#set $nfiles=5
#set $nfiles=40
#set $nfiles=200
#set $nfiles=500
#set $nfiles=1000
set $nfiles=1000
#set $meandirwidth=10000
set $meandirwidth=0
#set $filesize=cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)
set $filesize=4k
set $nthreads=1
set $iosize=1m
set $meanappendsize=4k
set $iters=1
set $eventrate=500

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=0,prealloc=80

define process name=multiclient1,devid=4,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop bwlimit name=bwlimit4
    flowop deletefile name=deletefile41,filesetname=bigfileset,fd=2

    flowop createfile name=createfile41,filesetname=bigfileset,fd=1
    flowop appendfile name=appendfile41,iosize=$meanappendsize,iters=$iters,fd=1
    flowop closefile name=closefile41,filesetname=bigfileset,fd=1
    flowop deletefile name=deletefile42,filesetname=bigfileset,fd=1 

    flowop createfile name=createfile42,filesetname=bigfileset,fd=2
    flowop appendfile name=appendfile42,iosize=$meanappendsize,iters=$iters,fd=2

    flowop fsync name=rsync4,filesetname=bigfileset,fd=2

    flowop closefile name=closefile42,filesetname=bigfileset,fd=2

  }
}

#debug 5

eventgen rate = $eventrate

#enable lathist

echo  "Single-process filemicro personality successfully loaded"

run 30

