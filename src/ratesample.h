#ifndef __UDT_RATESAMPLE_H__
#define __UDT_RATESAMPLE_H__

#include "buffer.h"

class RateSample
{
    // rate sample related info 
    int64_t prior_delivered_;
    int64_t prior_delivered_ts_;
    int64_t interval_;
    int64_t ack_elapsed_;
    int64_t send_elapsed_;
    float   delivery_rate_;

    // connection related info 
    int64_t cumu_delivered_;
    int64_t cumu_delivered_ts_;
    int64_t first_sent_ts_;

    bool    is_app_limited_;
    bool    packet_lost_;

    int64_t pkts_in_flight_;
    int     highest_seq_sent_;
    int     highest_ack_;

    void updateRateSample(Block* block);

public:
    RateSample();
    void onAck(Block* block);
    void onPktSent(Block* block);
    void onTimeout();

    float   deliveryRate() const { return delivery_rate_; }
    int64_t cumuDelivered() const { return cumu_delivered_; }
    bool    isAppLimited() const { return is_app_limited_; }
    int64_t pktsInFlight() const { return pkts_in_flight_; }
    bool    packetLost() const { return packet_lost_; }

    void setPacketLost(bool lost) {
        packet_lost_ = lost;
    }
};

#endif
