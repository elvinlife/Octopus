#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <thread>
#include <udt.h>
#include "cc.h"
#include "cbbr.h"
#include "test_util.h"
#include "common.h"

using namespace std;
using namespace std::chrono;

void* monitor(void*);

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

    pthread_create(new pthread_t, NULL, monitor, &client);

    FrameInfo msg;
    int     gop_size = 0;
    int     num_layers = 3;
    vector<FrameInfo> trace_array;
    std::ifstream ifs( argv[3] );
    ifs >> gop_size;
    while( ifs >> msg.msg_id_ >> msg.layer_id_ >> \
            msg.size_ >> msg.rate_ >> msg.ssim_ ) {
        trace_array.push_back( msg );
    }
    ifs.close();

    uint32_t        frame_no = 0;
    uint32_t        gop_no = 0;
    unsigned int    frame_gap = 33;
    int             bbr_rate = 0; 
    UDT::TRACEINFO perf;

    while (true) {
        int64_t ts_begin = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
        int64_t ts;

        if (UDT::ERROR == UDT::perfmon(client, &perf))
        {
            cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }
        bbr_rate = (int)1500 * 8.0 / perf.usPktSndPeriod * 1024;

        for (int i = 0; i < num_layers; ++i) {
            int msg_no = frame_no * num_layers + i;
            msg = trace_array[ msg_no % trace_array.size() ];

            if (perf.m_iBBRMode == 2 && msg.rate_ > bbr_rate)
                continue;

            uint32_t wildcard = msg.layer_id_ << 29 | msg.rate_;
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
            fprintf( stderr, "send_msg: msg_no: %u wildcard: %x ssim: %.2f ts: %ldms rate: %dkbps\n",
                    msg_no,
                    wildcard,
                    msg.ssim_,
                    ts,
                    bbr_rate
                    );
        }

        frame_no ++;
        if ( (ts-ts_begin)/1000 < frame_gap ) {
            std::this_thread::sleep_for( milliseconds( frame_gap - (ts-ts_begin)) );
        }
        if (frame_no % gop_size == 0) {
            gop_no += 1;
        }
    }

    UDT::close(client);
    return 0;
}

void* monitor(void* s)
{
    /*
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

   while (true)
   {
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      fprintf(stdout, "%8.5f\t\t%6.2f\t\t%d\t\t%f\t%d\t%d\n",
              perf.mbpsSendRate,
              perf.msRTT,
              perf.pktCongestionWindow,
              perf.usPktSndPeriod,
              perf.pktRecvACK,
              perf.pktRecvNAK);

   }
   */

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
