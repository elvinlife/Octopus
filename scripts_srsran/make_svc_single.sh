#!/bin/bash
VIDEOS=("mot17-10" "mot17-11" "mot17-12" "mot20-02" "mot20-07")
VTRACE=("../trace/mot17-10-quality.trace" "../trace/mot17-11-quality.trace" \
  "../trace/mot17-12-quality.trace" "../trace/mot20-02-quality.trace" \
  "../trace/mot20-07-quality.trace")
DELAYS=("30" "60")
NETWORKS=("attdown" "tmobiledown" "verizondown")
CQI_TRACES=("$HOME/Research/srsRAN-release_21_10/config/att-srsran-cqi.trace" \
  "$HOME/Research/srsRAN-release_21_10/config/tmobile-srsran-cqi.trace" \
  "$HOME/Research/srsRAN-release_21_10/config/verizon-srsran-cqi.trace")
UE_IP="172.16.0.2"
PORT="14000"

for i in $(seq 0 0); do
  for j in $(seq 0 0); do
    for k in $(seq 0 0); do
      LOG_PREFIX="${HOME}/Research/Octopus-Data/svc-single/${VIDEOS[i]}/${NETWORKS[k]}_delay${DELAYS[j]}"
      cd $HOME/Research/srsRAN-release_21_10/scripts
      ./start_testbed.sh ${CQI_TRACES[k]}
      # start the srsran testbed
      sleep 3
      cd $HOME/Research/Octopus/scripts_srsran
      sudo ip netns exec ue1 timeout 300 ../app/bbrserver $PORT > ${LOG_PREFIX}/octopus_recv.log 2> /dev/null &
      MM_DELAY=$(expr ${DELAYS[j]} - 10)
      mm-delay $MM_DELAY ../app/svcclient $UE_IP $PORT ${VTRACE[i]} > ${LOG_PREFIX}/octopus_send.log 2> /dev/null
      PORT=$(expr $PORT + 1)
      sleep 60
    done
  done
done
