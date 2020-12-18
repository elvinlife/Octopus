#ifndef __UDT_RATESAMPLE_H__
#define __UDT_RATESAMPLE_H__

#include "buffer.h"

class RateSample
{
private:
    // rate sample related info 
    uint64_t prior_delivered_;
    uint64_t prior_delivered_ts_;
    uint64_t interval_;
    uint64_t ack_elapsed_;
    uint64_t send_elapsed_;
    float   delivery_rate_;

    // connection related info 
    uint64_t cumu_delivered_;
    uint64_t cumu_delivered_ts_;
    uint64_t first_sent_ts_;

    bool    is_app_limited_;
    bool    packet_lost_;

    uint64_t pkts_in_flight_;
    int     highest_seq_sent_;
    int     highest_ack_;

    static  const int PacketMTU = 1500;
    void updateRateSample(Block* block, bool real_ack);

public:
    RateSample();
    void onAck(Block* block, bool real_ack);
    void onPktSent(Block* block, uint64_t send_ts);
    void onTimeout();

    float   deliveryRate() const { return delivery_rate_; }

    int64_t cumuDelivered() const { return cumu_delivered_; }

    bool    isAppLimited() const { return is_app_limited_; }

    int64_t pktsInFlight() const { return pkts_in_flight_; }

    bool    packetLost() const { return packet_lost_; }

    void setPacketLost(bool lost) {
        packet_lost_ = lost;
    }

    void setAppLimited(bool limited) {
        is_app_limited_ = limited;
    }
};

#endif
