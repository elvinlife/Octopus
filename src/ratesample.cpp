#include "ratesample.h"

RateSample::RateSample()
    :prior_delivered_(0),
    prior_delivered_ts_(0),
    interval_(0),
    ack_elapsed_(0),
    send_elapsed_(0),
    delivery_rate_(0),
    cumu_delivered_(0),
    cumu_delivered_ts_(CTimer::getTime()),
    first_sent_ts_(0)
{}

void RateSample::onAck(Block* block)
{
    // should update all pkts in sack_area
    /*
    fprintf(stderr, "rs onack, seq: %d, delivered_: %lu, delivered_ts: %lu, fisrt_sent_ts: %lu\n",
            block->seq_,
            block->delivered_,
            block->delivered_ts_,
            block->first_sent_ts_);
            */

    updateRateSample(block);
    if (prior_delivered_ts_ == 0)
        return;
    interval_ = send_elapsed_ > ack_elapsed_ ? send_elapsed_ : ack_elapsed_;
    int64_t sample_delivered = cumu_delivered_ - prior_delivered_;
    if (interval_ > 0) {
        delivery_rate_ = (float)sample_delivered / interval_ * 8;
        fprintf(stderr, "delivery_rate: %fMbps, sample_delivered: %ldbytes, send_elapsed: %ldms, ack_elapsed: %ldms\n", 
                delivery_rate_, 
                sample_delivered,
                send_elapsed_ / 1000, 
                ack_elapsed_ / 1000);
    }
}

void RateSample::onPktSent(Block* block)
{
    block->delivered_ = cumu_delivered_;
    block->delivered_ts_ = cumu_delivered_ts_;
    block->first_sent_ts_ = first_sent_ts_;
    block->sent_ts_ = CTimer::getTime();
}

void RateSample::updateRateSample(Block *block)
{
    if (block->delivered_ts_ == 0)
        return;
    cumu_delivered_ += (block->m_iLength + 20);
    cumu_delivered_ts_ = CTimer::getTime();
    if (block->delivered_ > prior_delivered_) {
        prior_delivered_ = block->delivered_;
        prior_delivered_ts_ = block->delivered_ts_;
        send_elapsed_ = block->sent_ts_ - block->first_sent_ts_;
        ack_elapsed_ = cumu_delivered_ts_ - block->delivered_ts_;
        first_sent_ts_ = block->sent_ts_;
    }
    block->delivered_ts_ = 0;
}
