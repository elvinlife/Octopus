#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACES=("../trace/mot17-10-temp.trace" "../trace/mot17-11-temp.trace" \
    "../trace/mot17-12-temp.trace" "../trace/mot20-02-temp.trace" \
    "../trace/mot20-07-temp.trace")
DELAYS=("30" "60")
NTRACES=("../trace/ATT-LTE-driving.down" \
    "../trace/TMobile-LTE-driving.down" \
    "../trace/Verizon-LTE-driving.down" )
DOWN_TRACE="../trace/24Mbps.trace"
NETWORKS=("attdown" "tmobiledown" "verizondown")
PORT="14000"
HOME_DIR="${HOME}/Octopus/scripts_mahimahi"

for i in $(seq 0 4); do
    for j in $(seq 0 1); do
        for k in $(seq 2 2); do
            LOG_DIR="${HOME_DIR}/aoi-anet/${VIDEOS[i]}/${NETWORKS[k]}_delay${DELAYS[j]}"
            MM_CMD="mm-delay ${DELAYS[j]} mm-link ${NTRACES[k]} ${DOWN_TRACE} --uplink-queue=dropactivenet --uplink-queue-args=\"packets=30,log_file=${LOG_DIR}/anet_30_link.log,\""
            timeout 300 ../app/bbrserver ${PORT} > ${LOG_DIR}/anet_30one_recv.log 2> /dev/null &
            $MM_CMD ./mm_aoi_send.sh ${PORT} ${VTRACES[i]} > ${LOG_DIR}/anet_30one_send.log 2> /dev/null &
            ((PORT++))
            sleep 20
        done
    done
done

