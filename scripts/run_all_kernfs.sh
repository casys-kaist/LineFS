#! /usr/bin/sudo /bin/bash
{
	(
		cd bench/micro || exit
		echo "scripts/reset_kernfs.sh $*"
		scripts/reset_kernfs.sh $*
	)
	exit
}
