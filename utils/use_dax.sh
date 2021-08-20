#! /bin/bash
if [[ $# < 1 ]];
then
	echo "$0 <bind or unbind>"
	exit
fi

if [[ $1 == "bind" ]];
then
	# sudo ndctl create-namespace -m dax -e namespace0.0 -f
	# sudo ndctl create-namespace -m dax -e namespace0.1 -f
	# sudo ndctl create-namespace -m dax -e namespace0.2 -f
	# sudo ndctl create-namespace -m dax -e namespace0.3 -f
	# sudo ndctl create-namespace -m dax -e namespace0.4 -f
	# sudo ndctl create-namespace -m dax -e namespace0.5 -f

	sudo ndctl create-namespace -m dax -f --size=96G -n namespace0.0
	sudo ndctl create-namespace -m dax -f --size=96G -n namespace0.1
	sudo ndctl create-namespace -m dax -f --size=96G -n namespace0.2
	sudo ndctl create-namespace -m dax -f --size=96G -n namespace0.3

	# sudo ndctl create-namespace -m dax -f --size=40G
	# sudo ndctl create-namespace -m dax -f --size=40G
	# sudo ndctl create-namespace -m dax -f --size=40G
	# sudo ndctl create-namespace -m dax -f --size=40G
	# sudo ndctl create-namespace -m dax -f --size=40G
	# sudo ndctl create-namespace -m dax -f --size=40G

	sudo chmod 777 /dev/dax0.0
	sudo chmod 777 /dev/dax0.1
	sudo chmod 777 /dev/dax0.2
	sudo chmod 777 /dev/dax0.3
	# sudo chmod 777 /dev/dax0.4
	# sudo chmod 777 /dev/dax0.5
elif [[ $1 == "unbind" ]];
then
	sudo ndctl create-namespace -m memory -e namespace0.0 -f
	sudo ndctl create-namespace -m memory -e namespace0.1 -f
	sudo ndctl create-namespace -m memory -e namespace0.2 -f
	sudo ndctl create-namespace -m memory -e namespace0.3 -f
	# sudo ndctl create-namespace -m memory -e namespace0.4 -f
	# sudo ndctl create-namespace -m memory -e namespace0.5 -f
else
	echo "$0 <bind or unbind>"
fi
