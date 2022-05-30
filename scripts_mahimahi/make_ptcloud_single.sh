#!/bin/bash
# For the volumetric video streaming baseline(ViVo), please refer to https://github.com/elvinlife/ssocket/tree/tcp

VTRACES=("../trace/dance.trace")
DELAYS=("30" "60")
NTRACES=("../trace/5G_static1_new.trace" "../trace/5G_static2_new.trace")
DOWN_TRACE="../trace/96Mbps.trace"
PORT="15001"

for i in $(seq 0 0); do
    for j in $(seq 0 0); do
        for k in $(seq 0 0); do
            LOG_PREFIX="${HOME}/Research/Octopus-Data/volumetric/delay${DELAYS[j]}net${k}"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropbitrate_dequeue --uplink-queue-args=\"packets=500,log_file=/tmp/dropbitrate.log,\""
            timeout 300 ../app/ptcloud_server ${PORT} > ${LOG_PREFIX}/octopus_recv.log 2> /dev/null &
            $MM_CMD ./mm_ptcloud_send.sh ${PORT} ${VTRACES[i]} > ${LOG_PREFIX}/octopus_send.log 2> ${LOG_PREFIX}/octopus_send.error
            #$MM_CMD ./mm_ptcloud_send.sh ${PORT} ${VTRACES[i]} > ${LOG_PREFIX}/octopus_send.log 2> /dev/null &
            ((PORT++))
        done
    done
done

