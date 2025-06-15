#!/bin/sh

serv=pml3
sstat=$(pidof $serv | wc -l )
if [ $sstat -gt 0 ]
then
echo "$serv is running fine!!!"
else
echo "$serv is down/dead"
echo "Starting $serv"
./pml3 &
fi