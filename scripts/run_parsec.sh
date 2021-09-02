#! /usr/bin/sudo /bin/bash
{
	(
		cd bench/micro || exit
		echo "scripts/parsec_ctl.sh $*"
		scripts/parsec_ctl.sh $*
	)
	exit
}