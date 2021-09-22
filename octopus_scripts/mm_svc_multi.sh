#!/bin/bash
../app/svcclient $MAHIMAHI_BASE $1 $3 > $4/octobbr_send1.log 2> /dev/null &
../app/svcclient $MAHIMAHI_BASE $2 $3 > $4/octobbr_send2.log 2> /dev/null
