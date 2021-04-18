#include "ratesample.h"
#include "common.h"

RateSample::RateSample()
    :prior_delivered_(0),
    prior_delivered_ts_(0),
    interval_(0),
    ack_elapsed_(0),
    send_elapsed_(0),
    delivery_rate_(0),
    cumu_delivered_(0),
    cumu_delivered_ts_(CTimer::getTime()),
    first_sent_ts_(0),
    is_app_limited_(false),
    packet_lost_(false),
    pkts_in_flight_(0),
    highest_seq_sent_(0),
    highest_ack_(0)
{}

void RateSample::onAck(Block* block, bool real_ack)
{
    // should update all pkts in sack_area
    delivered_mstamp_ = CTimer::getTime();
    acked_sacked_ = real_ack ? 1 : 0;
    if ( block->seq_ + 1 > highest_ack_ ) {
        highest_ack_ = block->seq_ + 1;
        pkts_in_flight_ = highest_seq_sent_ - highest_ack_ + 1;
    }
    setPacketLost( false );
    if ( !updateRateSample(block, real_ack) ||
            prior_delivered_ts_ == 0 )
        return;
    //interval_ = send_elapsed_ > ack_elapsed_ ? send_elapsed_ : ack_elapsed_;
    interval_ = ack_elapsed_;
    int64_t sample_delivered = cumu_delivered_ - prior_delivered_;
    if (interval_ > 0) {
        delivery_rate_ = (float)sample_delivered / interval_ / 0.128;
        fprintf(stderr, "delivery_rate: %.2fMbps sample_delivered: %ldB ack_elapsed_: %ldms send_elapsed_: %ldms \
                sent_ts: %ldms first_sent_ts: %ldms delivered_ts: %ldms inflight: %d\n", 
                delivery_rate_, 
                sample_delivered,
                ack_elapsed_ / 1000,
                send_elapsed_ / 1000,
                block->sent_ts_ / 1000,
                block->first_sent_ts_ / 1000,
                block->delivered_ts_ / 1000,
                pkts_in_flight_
                );
    }
    block->delivered_ts_ = 0;
}

void RateSample::onPktSent(Block* block, uint64_t send_ts)
{
    if ( block->seq_ > highest_seq_sent_ ) {
        highest_seq_sent_ = block->seq_;
        pkts_in_flight_ = highest_seq_sent_ - highest_ack_ + 1;
    }
    block->delivered_ = cumu_delivered_;
    block->delivered_ts_ = cumu_delivered_ts_;
    block->sent_ts_ = send_ts; 

    block->first_sent_ts_ = first_sent_ts_;
}

void RateSample::onTimeout()
{
    is_app_limited_ = false;
    packet_lost_ = false;
}

bool RateSample::updateRateSample(Block *block, bool real_ack)
{
    if (block->delivered_ts_ == 0)
        return false;
    // only update the ack_elapsed if the cumu_delivered of the acked block increases to avoid overestimation
    bool is_valid = block->delivered_ > prior_delivered_;

    cumu_delivered_ += acked_sacked_ * PacketMTU;
    cumu_delivered_ts_ = delivered_mstamp_;

    //if ( is_valid ) {
        prior_delivered_ = block->delivered_;
        prior_delivered_ts_ = block->delivered_ts_;
        send_elapsed_ = block->sent_ts_ - block->first_sent_ts_;
        ack_elapsed_ = cumu_delivered_ts_ - block->delivered_ts_;
        first_sent_ts_ = block->sent_ts_;
        return true;
    //}
    //return false;
}

/*
void RateSample::updateRateSample(Block *block, bool real_ack)
{
    if (block->delivered_ts_ == 0)
        return;

    cumu_delivered_ += acked_sacked_ * PacketMTU;
    cumu_delivered_ts_ = delivered_mstamp_;
    if ( block->delivered_ > prior_delivered_ ) {
        prior_delivered_ = block->delivered_;
        prior_delivered_ts_ = block->delivered_ts_;
        ack_elapsed_ = cumu_delivered_ts_ - block->delivered_ts_;
    }
}
*/
