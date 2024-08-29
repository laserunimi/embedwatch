#!/bin/sh

DEV=$1
FILE=$2
FILE_NAME=$3

cat $FILE |  base64 > tmp_file

stty -F $DEV 115200

printf "rm $FILE_NAME\n" > $DEV

for line in $(cat tmp_file);
do
        printf "echo $line >> $FILE_NAME\n"  > $DEV
done

#rm tmp_file

