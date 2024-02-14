#include "ratesample.h"
#include "common.h"

RateSample::RateSample()
    : interval_(0),
    ack_elapsed_(0),
    send_elapsed_(0),
    delivery_rate_(0),
    prior_delivered_(0),
    prior_delivered_ts_(0),
    cumu_delivered_(0),
    cumu_delivered_ts_(CTimer::getTime()),
    first_sent_ts_(CTimer::getTime()),
    app_limited_(0),
    packet_lost_(false),
    pkts_in_flight_(0),
    highest_seq_sent_(0),
    highest_ack_(0)
{}

void RateSample::onAckSacked(Block* block, int type)
{
    // this pkt has been sacked or acked
    if ( block->delivered_ts_ == 0 ||
            block->seq_ < highest_ack_
            )
        return;
    delivered_mstamp_ = CTimer::getTime();
    setPacketLost( false );
    if ( type == 1 ) {
        highest_ack_ = block->seq_ + 1;
        pkts_in_flight_ = highest_seq_sent_ - highest_ack_ + 1;
        acked_sacked_ = 1;
    }
    else if ( type == 2 ) {
        pkts_in_flight_ -= 1;
        acked_sacked_ = 1;
        setPacketLost( true );
    }
    else {
        acked_sacked_ = 0;
        setPacketLost( true );
    }
    updateRateSample( block );

    // when pkt i is acked, there should be a pkt acked when i is sent
    // to calculate ack rate
    if ( prior_delivered_ts_ == 0 )
        return;

    //interval_ = send_elapsed_ > ack_elapsed_ ? send_elapsed_ : ack_elapsed_;
    interval_ = ack_elapsed_;
    int64_t sample_delivered = cumu_delivered_ - prior_delivered_;
    if (interval_ > 0) {
        delivery_rate_ = (float)sample_delivered / interval_ * 8;
        
    fprintf(stderr, "delivery_rate: %.2fMbps sample_delivered: %ldB ack_elapsed_: %ldms send_elapsed_: %ldms "
            "sent_ts: %ldms first_sent_ts: %ldms delivered_ts: %ldms inflight: %d"
            "cumu_delivered: %ldB prior_delivered: %ldB\n", 
            delivery_rate_, 
            sample_delivered,
            ack_elapsed_ / 1000,
            send_elapsed_ / 1000,
            block->sent_ts_ / 1000,
            block->first_sent_ts_ / 1000,
            block->delivered_ts_ / 1000,
            pkts_in_flight_,
            cumu_delivered_,
            prior_delivered_
            );    
    }
    block->delivered_ts_ = 0;
}

void RateSample::onPktSent(Block* block, uint64_t send_ts)
{
    if ( block->seq_ > highest_seq_sent_ ) {
        highest_seq_sent_ = block->seq_;
        pkts_in_flight_ += 1;
    }
    block->delivered_ = cumu_delivered_;
    block->delivered_ts_ = cumu_delivered_ts_;
    block->sent_ts_ = send_ts; 
    block->first_sent_ts_ = first_sent_ts_;
}

void RateSample::onTimeout( int ack )
{
    app_limited_ = 0;
    packet_lost_ = false;
    // not to consider retransmitted packets for bandwidth estimation
    highest_ack_ = ack;

    prior_delivered_ = 0;
    prior_delivered_ts_ = 0;
    cumu_delivered_ = 0;
    cumu_delivered_ts_ = CTimer::getTime();
    first_sent_ts_ = CTimer::getTime();
}

bool RateSample::updateRateSample(Block *block)
{
    // only update the ack_elapsed if the cumu_delivered of the acked block increases to avoid overestimation
    bool is_valid = block->delivered_ > prior_delivered_;

    //cumu_delivered_ += acked_sacked_ * PacketMTU;
    cumu_delivered_ += acked_sacked_ * (block->m_iLength + 52); // application header(24) + udp(8) + ip(20)
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
