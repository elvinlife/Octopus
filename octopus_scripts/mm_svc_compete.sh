#!/bin/bash
../app/svcclient $MAHIMAHI_BASE $1 $3 > $4/octopus_send.log 2> /dev/null &
sleep 5 && ${HOME}/related/ssocket/src/client/sender $MAHIMAHI_BASE $2 &> $4/octopus_side_send.log
