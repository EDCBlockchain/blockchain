#!/bin/bash

echo 'Database path:' $1
data_dir=$1
#echo $data_dir_regx

# get pid of data/block db witness_node proc
pid=$(pgrep -f "=../$data_dir")
echo 'PID of witness_node process:' $pid

# get command
command=$(ps -p $pid -o args --no-headers)

# stop proc
echo 'Stopping a process...'
echo $command
kill -INT $pid 

echo 'Preparing db...'
while [ -d "$data_dir/blockchain/dblock" ]
do
sleep 5
done

# create temp dir '1' and copy db to temp dir
[ -d "1" ] && rm -rf 1
mkdir 1
cp -rf "$data_dir" 1/data

# remove config file
rm 1/data/config.ini

#empty p2p folder
rm -rf 1/data/p2p

cd 1
# make archive
echo 'Archiving...'
tar -cf data.tar data/

# remove db
rm -rf data

# start node if need
for arg in "$@"
do
if [ "$arg" == "-r" ]
then

# detect screen name
s_name=$(echo $data_dir | grep -o "/.*" | tr --delete '/')

echo 'Runing command: '$command' in screen '$s_name
screen -S $s_name -p 0 -X stuff "$command^M"
echo 'Successfully!'
fi
done

