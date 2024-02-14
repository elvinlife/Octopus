## Case study one(real-time video with frame rate adaptation)

`make_aoi_single.sh`: start one octopus flow, and the router applies octopus in-network content adaptation(the queue type in mahimahi is dropbitrate\_dequeue)

`make_aoi_anet.sh`: start one octopus flow, and the router is active network switch\[1\](the queue type in mahimahi is dropactivenet)

## Case study two(real-time video with quality adaptation)

`make_svc_single.sh`: start one octopus flow, and the router applies octopus in-network content adaptation

`make_svc_anet.sh`: start one octopus flow, and the router is active network switch

`make_svc_bottleneck.sh`: start one octopus flow, and there are two bottleneck links (one cellular switch, and one drop-tail legacy switch)

`make_svc_compete.sh`: start one octopus flow and one backlogged BBR flow, and they share a cellular switch

`make_svc_multi.sh`: start two octopus flows, and the router applies octopus in-network content adaptation

## Case study three(real-time volumetric video)

`make_ptcloud_single.sh`: start one octopus flow, and the router applies octopus drop logic

[1]: Samrat Bhattacharjee, Kenneth L. Calvert, and Ellen W. Zegura. 1997. An Archi- tecture for Active Networking. In Proceedings of the IFIP TC6 Seventh Interna- tional Conference on High Performance Netwoking VII (White Plains, New York, USA) (HPN ’97) 
