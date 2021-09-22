#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACES=("../trace/mot17-10-temp.trace" "../trace/mot17-11-temp.trace" \
    "../trace/mot17-12-temp.trace" "../trace/mot20-02-temp.trace" \
    "../trace/mot20-07-temp.trace")
DELAYS=("30" "10" "60")
NTRACES=("../trace/ATT-LTE-driving.down" \
    "../trace/TMobile-LTE-driving.down" \
    "../trace/Verizon-LTE-driving.down" )
DOWN_TRACE="../trace/24Mbps.trace"
NETWORKS=("attdown" "tmobiledown" "verizondown")
PORT="14000"

for i in $(seq 0 4); do
    for j in $(seq 2 2); do
        for k in $(seq 2 2); do
            LOG_PREFIX="${HOME}/Research/Octopus-Data/aoi-anet2/${VIDEOS[i]}/"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropactivenet --uplink-queue-args=\"packets=30,log_file=${LOG_PREFIX}/anet_30_link.log,\""
            timeout 300 ../app/bbrserver ${PORT} > ${LOG_PREFIX}/anet_30one_recv.log 2> /dev/null &
            $MM_CMD ./mm_aoi_send.sh ${PORT} ${VTRACES[i]} > ${LOG_PREFIX}/anet_30one_send.log 2> /dev/null &
            ((PORT++))
            sleep 20
        done
    done
done

