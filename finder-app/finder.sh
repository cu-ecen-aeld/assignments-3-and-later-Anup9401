#!/bin/sh

if [ $# -eq 2 ]
then
	filesdir=$1
	searchstr=$2
elif [ $# -eq 1 ]
then
	printf "searchstr not entered\n"
	exit 1
elif [ $# -eq 0 ]
then
	printf "filesdir and searchstr not entered\n"
	exit 1
fi

if [ -d $filesdir ]
then
	x=$(find "$filesdir" -type f|wc -l)
	y=$(grep "$searchstr" $(find "$filesdir" -type f)|wc -l)
else
	printf "Invalid filesdir\n"
	exit 1
fi

printf "The number of files are $x and the number of matching lines are $y\n"

