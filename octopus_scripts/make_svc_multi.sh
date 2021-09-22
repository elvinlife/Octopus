#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACES=("../trace/mot17-10-quality.trace" "../trace/mot17-11-quality.trace" \
    "../trace/mot17-12-quality.trace" "../trace/mot20-02-quality.trace" \
    "../trace/mot20-07-quality.trace")
DELAYS=("10" "30" "60")
NTRACES=( "../trace/12Mbps.trace" \
    "../trace/ATT-LTE-driving.down" \
    "../trace/TMobile-LTE-driving.down" \
    "../trace/Verizon-LTE-driving.down" )

DOWN_TRACE="../trace/24Mbps.trace"
NETWORKS=("const" "attdown" "tmobiledown" "verizondown")
PORT1="24000"
PORT2="25000"

sudo sysctl net.ipv4.tcp_congestion_control=bbr
for i in $(seq 0 4); do
    for j in $(seq 1 2); do
        for k in $(seq 1 3); do
            LOG_PREFIX="${HOME}/Research/Octopus-Data/svc-multi/${VIDEOS[i]}/${NETWORKS[k]}_delay${DELAYS[j]}"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropbitrate_dequeue --uplink-queue-args=\"packets=250,log_file=${LOG_PREFIX}/octopus_link.log,\""
            timeout 300 ../app/bbrserver ${PORT1} > ${LOG_PREFIX}/octopus1_recv.log 2> /dev/null &
            timeout 300 ../app/bbrserver ${PORT2} > ${LOG_PREFIX}/octopus2_recv.log 2> /dev/null &
            $MM_CMD ./mm_svc_multi.sh ${PORT1} ${PORT2} ${VTRACES[i]} ${LOG_PREFIX} &
            ((PORT1++))
            ((PORT2++))
            sleep 60
        done
    done
done

