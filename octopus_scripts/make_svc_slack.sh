#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACES=("../trace/mot17-10-quality.trace" "../trace/mot17-11-quality.trace" \
    "../trace/mot17-12-quality.trace" "../trace/mot20-02-quality.trace" \
    "../trace/mot20-07-quality.trace")
DELAYS=("10" "30" "60")
NTRACES=("../trace/ATT-LTE-driving.down" \
    "../trace/TMobile-LTE-driving.down" \
    "../trace/Verizon-LTE-driving.down" )
DOWN_TRACE="../trace/24Mbps.trace"
NETWORKS=("attdown" "tmobiledown" "verizondown")
PORT="15000"

for i in $(seq 0 4); do
    for j in $(seq 1 2); do
        for k in $(seq 2 2); do
            LOG_PREFIX="${HOME}/Research/Octopus-Data/svc-slack/${VIDEOS[i]}/${NETWORKS[k]}_delay${DELAYS[j]}"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropbitrate_dequeue --uplink-queue-args=\"packets=250,log_file=${LOG_PREFIX}/const300_link.log,\""
            timeout 300 ../app/bbrserver ${PORT} > ${LOG_PREFIX}/octopus_const300_recv.log 2> /dev/null &
            $MM_CMD ./mm_svc_send.sh ${PORT} ${VTRACES[i]} > ${LOG_PREFIX}/octopus_const300_send.log 2> /dev/null &
            ((PORT++))
            sleep 50
        done
    done
done

