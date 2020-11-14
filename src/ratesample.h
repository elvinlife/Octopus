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

    void updateRateSample(Block* block);

public:
    RateSample();
    void onAck(Block* block);
    void onPktSent(Block* block);
};
