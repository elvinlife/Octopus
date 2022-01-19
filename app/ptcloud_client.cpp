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

const int MAX_CELLS = 2;
const int MAX_LAYERS = 10;

struct FrameInfo {
    int frame_id_;
    int sizes[MAX_LAYERS][MAX_CELLS];
    int rates[MAX_LAYERS];
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

    int num_layers, num_cells, num_frames;
    vector<FrameInfo> frame_array;
    std::ifstream ifs( argv[3] );
    ifs >> num_layers >> num_cells >> num_frames;
    for (int i = 0; i < num_frames; ++i) {
        FrameInfo frame;
        int fid, lid, cid, f_size, rate;
        for (int j = 0; j < num_layers; ++j) {
            for (int k = 0; k < num_cells; ++k) {
                ifs >> fid >> lid >> cid >> f_size >> rate;
                frame.frame_id_ = i;
                frame.sizes[lid][cid] = f_size;
                frame.rates[lid] = rate;
            }
        }
        frame_array.push_back(frame);
    }
    ifs.close();

    uint32_t    frame_no = 0;
    float       frame_gap = 50;
    int         bbr_rate = 0; 
    const uint32_t PREEMPT_MUSK = 0x2000000;
    UDT::TRACEINFO perf;
    uint64_t ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
    uint64_t ts = ts_begin;

    while (true) {
        if (UDT::ERROR == UDT::perfmon(client, &perf)) {
            cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }
        bbr_rate = perf.pacingRate;
        FrameInfo frame = frame_array[ frame_no % frame_array.size() ];
        int final_layer = 0;
        int final_rate = 0;
        int final_bytes = 0;
        for (int i = 0; i < num_layers; ++i) {
            if ( frame.rates[i] <= (int)bbr_rate ) {
                final_layer = i;
                final_rate = frame.rates[i];
            }
        }
        if (perf.appLimited >= 10 && final_layer < num_layers - 1) {
            final_layer += 1;
            final_rate = frame.rates[final_layer];
        }
        
        for (int i = 0; i <= final_layer; ++i) {
            uint32_t wildcard = 0;
            // the msgs of layer 0 are dropper messages, with priority 1
            if (i == 0) {
                wildcard |= 1 << 29 | PREEMPT_MUSK;
                if ( frame_no % 4 == 0 ) {
                    wildcard |= 1 << 26;
                }
                else {
                    wildcard |= ( 4 - frame_no % 4 ) << 26;
                }
            }
            // the msgs of layer 1-4 have priority 2
            else if ( i < 5 ) {
                wildcard |= 2 << 29;
            }
            // the msgs of layer 5 have priority 3
            else if ( i == 5) {
                wildcard |= 3 << 29;
            }
            // the msgs of layer 6-9 have priority 4
            else {
                wildcard |= 4 << 29;
            }
            wildcard |= (50 << 16);

            final_rate = frame.rates[i];
            for (int j = 0; j < num_cells; ++j) {
                final_bytes += frame.sizes[i][j];
                std::string meta_data = "frame: " + std::to_string(frame_no) + " layer: " + std::to_string(i) + " cell: " + std::to_string(j) + " ";
                int extra_dummy = frame.sizes[i][j] - meta_data.size();
                std::string msg_payload = meta_data + std::string(extra_dummy, '|');
                int ss = UDT::sendmsg( client, msg_payload.c_str(), msg_payload.size(), (1<<20), 0, wildcard );
                if ( ss != (int)msg_payload.size() || ss == UDT::ERROR ) {
                    fprintf(stderr, "Error, sendmsg size mismatch: %u, ss:%u\n", msg_payload.size(), ss);
                    exit(-1);
                }
            }
        }

        ts = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        fprintf( stdout, "send_msg frame_no: %u final_rate: %d kbps final_size: %d "
                "ts_send: %lu real_time: %.2fms\n",
                frame_no,
                final_rate,
                final_bytes,
                ts,
                CTimer::getTime() / 1000.0 );

        frame_no ++;
        if ( ts-ts_begin < (uint64_t)(frame_no * frame_gap) ) {
            std::this_thread::sleep_for( 
                milliseconds( (uint64_t)(frame_no * frame_gap) - (ts - ts_begin) ) );
        }
    }

    UDT::close(client);
    return 0;
}
