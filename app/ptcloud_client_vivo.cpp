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
#include <deque>

using namespace std;
using namespace std::chrono;

const int MAX_CELLS = 2;
const int MAX_LAYERS = 10;
const int MAX_BANDWIDTH = 300000;
const int MIN_BANDWIDTH = 12000;

struct FrameInfo {
    int frame_id_;
    int sizes[MAX_LAYERS][MAX_CELLS];
    int rates[MAX_LAYERS];
};

struct BWRecord {
    deque<int> array;
    int n_record;
    const int max_record = 20;

    BWRecord()
    : n_record(1) {
        array.push_back(MIN_BANDWIDTH);
    }

    int append(int bw) {
        if (n_record < max_record) {
            array.push_back(bw);
            n_record += 1;
        }
        else {
            array.pop_front();
            array.push_back(bw);
        }
    }

    int harmonic_mean() {
        if (array.size() == 0)
            return 0;
        double sum_inv = 0;
        for (int i = 0; i < array.size(); ++i) {
            sum_inv += 1.0 / array[i];
        }
        return (int)(array.size() / sum_inv);
    }
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
    BWRecord    bw_record = BWRecord();
    uint64_t    ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
    uint64_t    ts = ts_begin;

    while (true) {
        FrameInfo frame = frame_array[ frame_no % frame_array.size() ];
        int final_layer = 0;
        int final_rate = 0;
        int final_bytes = 0;
        for (int i = 0; i < num_layers; ++i) {
            if (frame.rates[i] <= bw_record.harmonic_mean()) {
                final_layer = i;
                final_rate = frame.rates[i];
            }
        }
        
        uint64_t before_send = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
        for (int i = 0; i <= final_layer; ++i) {
            uint32_t wildcard = 0;
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
        uint64_t after_send = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
        
        int downlink_rate = MAX_BANDWIDTH;
        if ( (after_send - before_send) > 0 ) {
            downlink_rate = final_bytes * 8 / (after_send - before_send);
        }
        bw_record.append(downlink_rate > MAX_BANDWIDTH ? MAX_BANDWIDTH : downlink_rate);

        ts = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        fprintf( stdout, "send_msg frame_no: %u final_rate: %d kbps final_size: %d B"
                "ts_send: %lu real_time: %.2fms downlink_rate: %d\n",
                frame_no,
                final_rate,
                final_bytes,
                ts,
                CTimer::getTime() / 1000.0,
                downlink_rate );

        frame_no ++;
        if ( ts-ts_begin < (uint64_t)(frame_no * frame_gap) ) {
            std::this_thread::sleep_for( 
                milliseconds( (uint64_t)(frame_no * frame_gap) - (ts - ts_begin) ) );
        }
        else {
            frame_no = (ts - ts_begin) / frame_gap;
        }
    }

    UDT::close(client);
    return 0;
}
