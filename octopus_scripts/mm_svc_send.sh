#!/bin/bash
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/Research/Octopus/src
../app/svcclient $MAHIMAHI_BASE $1 $2
