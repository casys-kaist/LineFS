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
set $iters=10
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

define fileset name=bigfileset2,path=$dir,size=$filesize,entries=$nfiles,dirwidth=0,prealloc=80

define process name=multiclient2,devid=5,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop bwlimit name=bwlimit5
    flowop deletefile name=deletefile51,filesetname=bigfileset2,fd=2

    flowop createfile name=createfile51,filesetname=bigfileset2,fd=1
    flowop appendfile name=appendfile51,iosize=$meanappendsize,iters=$iters,fd=1
    flowop closefile name=closefile51,filesetname=bigfileset2,fd=1
    flowop deletefile name=deletefile52,filesetname=bigfileset2,fd=1 

    flowop createfile name=createfile52,filesetname=bigfileset2,fd=2
    flowop appendfile name=appendfile52,iosize=$meanappendsize,iters=$iters,fd=2

    flowop fsync name=rsync5,filesetname=bigfileset2,fd=2

    flowop closefile name=closefile52,filesetname=bigfileset2,fd=2

  }
}

define fileset name=bigfileset3,path=$dir,size=$filesize,entries=$nfiles,dirwidth=0,prealloc=80

define process name=multiclient3,devid=6,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop bwlimit name=bwlimit6
    flowop deletefile name=deletefile61,filesetname=bigfileset3,fd=2

    flowop createfile name=createfile61,filesetname=bigfileset3,fd=1
    flowop appendfile name=appendfile61,iosize=$meanappendsize,iters=$iters,fd=1
    flowop closefile name=closefile61,filesetname=bigfileset3,fd=1
    flowop deletefile name=deletefile62,filesetname=bigfileset3,fd=1 

    flowop createfile name=createfile62,filesetname=bigfileset3,fd=2
    flowop appendfile name=appendfile62,iosize=$meanappendsize,iters=$iters,fd=2

    flowop fsync name=rsync6,filesetname=bigfileset3,fd=2

    flowop closefile name=closefile62,filesetname=bigfileset3,fd=2

  }
}

define fileset name=bigfileset4,path=$dir,size=$filesize,entries=$nfiles,dirwidth=0,prealloc=80

define process name=multiclient4,devid=7,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop bwlimit name=bwlimit7
    flowop deletefile name=deletefile71,filesetname=bigfileset4,fd=2

    flowop createfile name=createfile71,filesetname=bigfileset4,fd=1
    flowop appendfile name=appendfile71,iosize=$meanappendsize,iters=$iters,fd=1
    flowop closefile name=closefile71,filesetname=bigfileset4,fd=1
    flowop deletefile name=deletefile72,filesetname=bigfileset4,fd=1 

    flowop createfile name=createfile72,filesetname=bigfileset4,fd=2
    flowop appendfile name=appendfile72,iosize=$meanappendsize,iters=$iters,fd=2

    flowop fsync name=rsync7,filesetname=bigfileset4,fd=2

    flowop closefile name=closefile72,filesetname=bigfileset4,fd=2

  }
}

#debug 5

eventgen rate = $eventrate

enable lathist

echo  "Multi-process filemicro personality successfully loaded"

run 30

