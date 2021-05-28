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

/*
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


    FrameInfo msg;
    int     gop_size = 0;
    int     num_layers = 3;
    vector<FrameInfo> trace_array;
    std::ifstream ifs( argv[3] );
    ifs >> gop_size >> num_layers;
    while( ifs >> msg.msg_id_ >> msg.layer_id_ >> \
            msg.size_ >> msg.rate_ >> msg.ssim_ ) {
        msg.rate_ = (int)msg.rate_ * 1500.0 /1400.0;
        trace_array.push_back( msg );
    }
    ifs.close();

    uint32_t    frame_no = 0;
    uint32_t    gop_no = 0;
    int         frame_gap = 33;

    while (true) {
        int64_t ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        int64_t ts = ts_begin;

        for (int i = 0; i < num_layers; ++i) {
            int msg_no = frame_no * num_layers + i;
            msg = trace_array[ msg_no % trace_array.size() ];

            uint32_t wildcard = (msg.layer_id_+1) << 29 | msg.rate_;
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
                    msg_no,
                    msg.size_,
                    wildcard,
                    msg.ssim_,
                    ts
                    );
        }

        frame_no ++;
        if ( (ts-ts_begin) < frame_gap ) {
            std::this_thread::sleep_for( milliseconds( frame_gap - (ts-ts_begin)) );
        }
        if (frame_no % gop_size == 0) {
            gop_no += 1;
        }
    }

    UDT::close(client);
    return 0;
}
*/

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
    UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CBBR>, sizeof(CCCFactory<CBBR>));
    UDT::setsockopt(client, 0, UDT_SNDSYN, new bool(true), sizeof(bool));

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

    FrameInfo msg;
    int     gop_size = 0;
    int     num_layers = 3;
    int     max_bitrate = 0;

    //vector<FrameInfo> trace_array;
    map<int, vector<FrameInfo>> trace_arrays;
    std::ifstream ifs( argv[3] );
    ifs >> gop_size >> num_layers;
    while( ifs >> max_bitrate >> \
            msg.msg_id_ >> \
            msg.layer_id_ >> \
            msg.size_ >> \
            msg.rate_ >> \
            msg.ssim_ ) {
        max_bitrate = (int) max_bitrate * 15.0 / 14.0;
        msg.rate_ = (int)msg.rate_ * 1500.0 /1400.0;
        if ( trace_arrays.find(max_bitrate) == trace_arrays.end() ) {
            trace_arrays[max_bitrate] = vector<FrameInfo>();
        }
        trace_arrays[max_bitrate].push_back( msg );
    }
    ifs.close();

    uint32_t    frame_no = 0;
    uint32_t    gop_no = 0;
    uint64_t    frame_gap = 33;
    int         bbr_rate = 0; 
    int         key_trace = 0;
    int         smallest_key = 1 << 20;
    const uint32_t PREEMPT_MUSK = 0x10000000;
    UDT::TRACEINFO perf;

    while (true) {
        if (frame_no % gop_size == 0) {
            if (UDT::ERROR == UDT::perfmon(client, &perf))
            {
                cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
                break;
            }
            gop_no += 1;
            bbr_rate = perf.pacingRate;
            key_trace = 0;
            for (auto it = trace_arrays.begin(); it != trace_arrays.end(); ++it) {
                if ( it->first < bbr_rate ) {
                    key_trace = it->first;
                }
                if ( it->first < smallest_key )
                    smallest_key = it->first;
            }
            if ( key_trace == 0 )
                key_trace = smallest_key;
            fprintf( stdout, "set video level: bitrate: %d, bbr_rate: %d\n",
                    key_trace, bbr_rate );
        }

        uint64_t ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        uint64_t ts = ts_begin;

        for (int i = 0; i < num_layers; ++i) {
            int msg_no = frame_no * num_layers + i;
            msg = trace_arrays[key_trace][ msg_no % trace_arrays[key_trace].size() ];

            uint32_t wildcard = (msg.layer_id_+1) << 29 | msg.rate_;
            if ( i == 0 )
                wildcard = (msg.layer_id_+1) << 29;
            if ( frame_no % gop_size == 0 && i == 0) {
                wildcard = wildcard | PREEMPT_MUSK;
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
            fprintf( stdout, "send_msg msg_no: %u size: %d layer_id: %d ssim: %.2f ts_send: %lu key_trace: %d frame_no: %d real_time: %.2fms\n",
                    msg_no,
                    msg.size_,
                    msg_no % num_layers,
                    msg.ssim_,
                    ts,
                    key_trace,
                    msg_no / num_layers,
                    CTimer::getTime() / 1000.0 );
        }

        frame_no ++;
        if ( ts-ts_begin < frame_gap ) {
            std::this_thread::sleep_for( milliseconds( frame_gap - (ts-ts_begin) ) );
        }
    }

    UDT::close(client);
    return 0;
}
