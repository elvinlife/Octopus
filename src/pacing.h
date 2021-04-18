#ifndef __UDT_PACING_H__
#define __UDT_PACING_H__
#include <deque>

class Pacing
{
private:
    std::deque<uint64_t> send_queue_;
    float send_rate_;
public:
    Pacing( void )
        : send_queue_(), send_rate_(0) {}

    void set_send_rate( float send_rate ) { send_rate_ = send_rate; }


}
