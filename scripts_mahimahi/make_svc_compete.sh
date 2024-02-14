#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACES=("../trace/mot17-10-quality.trace" "../trace/mot17-11-quality.trace" \
    "../trace/mot17-12-quality.trace" "../trace/mot20-02-quality.trace" \
    "../trace/mot20-07-quality.trace")
DELAYS=("30" "60")
NTRACES=( "../trace/12Mbps.trace" \
    "../trace/ATT-LTE-driving.down" \
    "../trace/TMobile-LTE-driving.down" \
    "../trace/Verizon-LTE-driving.down" )

DOWN_TRACE="../trace/24Mbps.trace"
NETWORKS=("const" "attdown" "tmobiledown" "verizondown")
PORT="24000"
TCP_PORT="25000"

sudo sysctl net.ipv4.tcp_congestion_control=bbr
for i in $(seq 0 4); do
    for j in $(seq 0 1); do
        for k in $(seq 0 3); do
            LOG_DIR="${HOME}/Research/Octopus-Data/svc-compete/${VIDEOS[i]}/${NETWORKS[k]}_delay${DELAYS[j]}"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropbitrate_dequeue --uplink-queue-args=\"packets=250,log_file=${LOG_DIR}/octopus_link.log,\""
            timeout 300 ../app/bbrserver ${PORT} > ${LOG_DIR}/octopus_recv.log 2> /dev/null &
            timeout 300 ${HOME}/related/ssocket/src/client/receiver ${TCP_PORT} &> /dev/null &
            $MM_CMD ./mm_svc_compete.sh ${PORT} ${TCP_PORT} ${VTRACES[i]} ${LOG_DIR} &
            ((PORT++))
            ((TCP_PORT++))
            sleep 60
        done
    done
done

