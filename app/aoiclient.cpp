#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <thread>
#include <udt.h>
#include <map>
#include "cc.h"
#include "cbbr.h"
#include "test_util.h"
#include "common.h"

using namespace std;
using namespace std::chrono;


struct FrameInfo {
    int msg_id_;
    uint32_t layer_id_;
    int size_;
    float ssim_;
    uint32_t rate_;
};

int main(int argc, char* argv[])
{
    if ((4 != argc) || (0 == atoi(argv[2])))
    {
        cout << "usage: appclient server_ip server_port trace_file" << endl;
        return 0;
    }

    // Automatically start up and clean up UDT module.
    UDTUpDown _udt_;

    struct addrinfo hints, *local, *peer;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (0 != getaddrinfo(NULL, "9000", &hints, &local))
    {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }

    UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CTCP>, sizeof(CCCFactory<CTCP>));
    UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CBBR>, sizeof(CCCFactory<CBBR>));
    UDT::setsockopt(client, 0, UDT_SNDSYN, new bool(true), sizeof(bool));
    //UDT::setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
    //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client, 0, UDT_MAXBW, new int64_t(12500000), sizeof(int));

    freeaddrinfo(local);

    if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
    {
        cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
        return 0;
    }

    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
    {
        cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(peer);

    FrameInfo   msg;
    int         gop_size = 0;
    vector<FrameInfo> trace_array;
    std::ifstream ifs( argv[3] );
    ifs >> gop_size;
    while( ifs >> msg.msg_id_ >> msg.size_ >> msg.ssim_ ) {
        trace_array.push_back( msg );
    }
    ifs.close();

    uint32_t    frame_no = 0;
    uint32_t    PREEMPT_MUSK = 0x2000000;
    float       frame_gap = 33.333;
    int64_t ts, ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();

    while (true) {
        msg = trace_array[ frame_no % trace_array.size() ];
        uint32_t wildcard = 0;
        if (frame_no % gop_size == 0) {
            wildcard = 1 << 29 | 1 << 26 | PREEMPT_MUSK;
        }
        else {
            wildcard = 2 << 29 | 2 << 26 | PREEMPT_MUSK;
        }

        std::string msg_payload( msg.size_, 'a' );
        int ss = 0;

        if (UDT::ERROR == ( ss = UDT::sendmsg( client, msg_payload.c_str(), msg_payload.size(), (1<<20), 0, wildcard ) ) ) {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            exit( -1 );
        }
        if ( ss != msg.size_ ) {
            fprintf(stderr, "Error, sendmsg size mismatch: %u, ss:%u\n", msg.size_, ss);
            exit(0);
        }

        ts = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        fprintf( stdout, "send_msg msg_no: %u size: %d wildcard: %x ssim: %.2f ts_send: %ld\n",
                frame_no,
                msg.size_,
                wildcard,
                msg.ssim_,
                ts);

        frame_no ++;
        if ( (ts-ts_begin) < (uint64_t)(frame_no * frame_gap) ) {
            std::this_thread::sleep_for( milliseconds( (uint64_t)(frame_no * frame_gap) - (ts-ts_begin)) );
        }
    }

    UDT::close(client);
    return 0;
}
