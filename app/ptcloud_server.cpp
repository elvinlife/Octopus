#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include <udt.h>
#include <chrono>
#include "cc.h"
#include "test_util.h"
#include "common.h"

using namespace std;
using namespace std::chrono;

#ifndef WIN32
void* recvdata(void*);
#else
DWORD WINAPI recvdata(LPVOID);
#endif

struct PacketHeader {
    int32_t header_[5];
    PacketHeader( char* buf ) {
        memcpy (header_, buf, 20);
    }

    int32_t seq() { return header_[0]; }
    int32_t msgno() { return header_[1] & 0x1FFFFFFF; }
    uint32_t wildcard() { return (uint32_t)header_[2]; }
};

int main(int argc, char* argv[])
{
   if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
   {
      cout << "usage: appserver [server_port]" << endl;
      return 0;
   }

   // Automatically start up and clean up UDT module.
   UDTUpDown _udt_;

   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   //hints.ai_socktype = SOCK_STREAM;
   hints.ai_socktype = SOCK_DGRAM;

   string service("9000");
   if (2 == argc)
      service = argv[1];

   if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return 0;
   }

   UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   // UDT Options
   UDT::setsockopt(serv, 0, UDT_RCVSYN, new bool(true), sizeof(bool));
   //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CTCP>, sizeof(CCCFactory<CTCP>));
   //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

   if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(res);

   cout << "server is ready at port: " << service << endl;

   if (UDT::ERROR == UDT::listen(serv, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   UDTSOCKET recver;

   while (true)
   {
      if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen)))
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      char clienthost[NI_MAXHOST];
      char clientservice[NI_MAXSERV];
      getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
      cout << "new connection: " << clienthost << ":" << clientservice << endl;

      pthread_t rcvthread;
      pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
      pthread_detach(rcvthread);
   }

   UDT::close(serv);

   return 0;
}

#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   char* data;
   int size = 100000;
   data = new char[size];

   while (true)
   {
       int rcv_size;
       int var_size = sizeof(int);
       UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
       int ss = 0;
       if (UDT::ERROR == ( ss = UDT::recvmsg(recver, data, size)) )
       {
           cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
           break;
       }
      PacketHeader header(data);

      char meta_data[40];
      memcpy(meta_data, data + 24, 30);
      meta_data[39] = 0;
      //fprintf( stdout, "meta_data: %s\n", meta_data);

      int64_t ts = duration_cast< milliseconds >( system_clock::now().time_since_epoch() ).count();
      fprintf( stdout, "recv_msg msg_no: %d wildcard: %x size: %d %s ts_recv: %lu\n",
               header.msgno(),
               header.wildcard(),
               ss,
               meta_data,
               ts
               );
   }

   delete [] data;

   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
