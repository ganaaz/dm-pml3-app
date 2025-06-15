#!/bin/sh
export LOG4C_RCPATH=${HOME}/log4crc

running=1

finish()
{
    running=0
}

trap finish SIGINT

while [ $running -eq 1 ]
do
    /home/app3/autorun.sh
    sleep 30
done

#start the l3 app
#./l3app &
