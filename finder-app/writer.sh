#!/bin/sh

if [ $# -eq 2 ]
then
	writefile=$1
	writestr=$2
elif [ $# -eq 1 ]
then
	printf "writestr not entered\n"
	exit 1
elif [ $# -eq 0 ]
then
	printf "writefile and writestr not entered\n"
	exit 1
fi
	
dir=$(dirname "$writefile")
mkdir -p $dir
echo $writestr > $writefile

if [ $? -eq 1 ]
then
	printf "File not created\n"
	exit 1	
fi


